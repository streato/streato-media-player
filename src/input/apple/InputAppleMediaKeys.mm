//
// Created by Tobias Hieta on 21/08/15.
//

#include "InputAppleMediaKeys.h"
#include "SPMediaKeyTap.h"
#include "QsLog.h"

#import <dlfcn.h>

#import <MediaPlayer/MediaPlayer.h>

@interface MediaKeysDelegate : NSObject
{
  SPMediaKeyTap* keyTap;
  InputAppleMediaKeys* input;
}
-(instancetype)initWithInput:(InputAppleMediaKeys*)input;
@end

@implementation MediaKeysDelegate

- (instancetype)initWithInput:(InputAppleMediaKeys*)input_
{
  self = [super init];
  if (self) {
    input = input_;
    if (NSClassFromString(@"MPRemoteCommandCenter")) {
      MPRemoteCommandCenter* center = [MPRemoteCommandCenter sharedCommandCenter];
#define CONFIG_CMD(name) \
  [center.name ## Command addTarget:self action:@selector(gotCommand:)]
      CONFIG_CMD(play);
      CONFIG_CMD(pause);
      CONFIG_CMD(togglePlayPause);
      CONFIG_CMD(stop);
      CONFIG_CMD(nextTrack);
      CONFIG_CMD(previousTrack);
      CONFIG_CMD(seekForward);
      CONFIG_CMD(seekBackward);
      CONFIG_CMD(skipForward);
      CONFIG_CMD(skipBackward);
      [center.changePlaybackPositionCommand addTarget:self action:@selector(gotPlaybackPosition:)];
    } else {
      keyTap = [[SPMediaKeyTap alloc] initWithDelegate:self];
      if ([SPMediaKeyTap usesGlobalMediaKeyTap])
        [keyTap startWatchingMediaKeys];
      else
        QLOG_WARN() << "Could not grab global media keys";
    }
  }
  return self;
}

- (void)dealloc
{
  [super dealloc];
}

-(MPRemoteCommandHandlerStatus)gotCommand:(MPRemoteCommandEvent *)event
{
  QString keyPressed;
  MPRemoteCommand* command = [event command];

#define CMD(name) [MPRemoteCommandCenter sharedCommandCenter].name ## Command
  if (command == CMD(play)) {
    keyPressed = INPUT_KEY_PLAY;
  } else if (command == CMD(pause)) {
    keyPressed = INPUT_KEY_PAUSE;
  } else if (command == CMD(togglePlayPause)) {
    keyPressed = INPUT_KEY_PLAY_PAUSE;
  } else if (command == CMD(stop)) {
    keyPressed = INPUT_KEY_STOP;
  } else if (command == CMD(nextTrack)) {
    keyPressed = INPUT_KEY_NEXT;
  } else if (command == CMD(previousTrack)) {
    keyPressed = INPUT_KEY_PREV;
  } else {
    return MPRemoteCommandHandlerStatusCommandFailed;
  }

  emit input->receivedInput("AppleMediaKeys", keyPressed, InputBase::KeyPressed);
  return MPRemoteCommandHandlerStatusSuccess;
}

-(MPRemoteCommandHandlerStatus)gotPlaybackPosition:(MPChangePlaybackPositionCommandEvent *)event
{
  PlayerComponent::Get().seekTo(event.positionTime * 1000);
  return MPRemoteCommandHandlerStatusSuccess;
}

-(void)mediaKeyTap:(SPMediaKeyTap *)keyTap receivedMediaKeyEvent:(NSEvent *)event
{
  int keyCode = (([event data1] & 0xFFFF0000) >> 16);
  int keyFlags = ([event data1] & 0x0000FFFF);
  BOOL keyIsPressed = (((keyFlags & 0xFF00) >> 8)) == 0xA;

  QString keyPressed;

  switch (keyCode) {
    case NX_KEYTYPE_PLAY:
      keyPressed = INPUT_KEY_PLAY_PAUSE;
      break;
    case NX_KEYTYPE_FAST:
      keyPressed = "KEY_FAST";
      break;
    case NX_KEYTYPE_REWIND:
      keyPressed = "KEY_REWIND";
      break;
    case NX_KEYTYPE_NEXT:
      keyPressed = INPUT_KEY_NEXT;
      break;
    case NX_KEYTYPE_PREVIOUS:
      keyPressed = INPUT_KEY_PREV;
      break;
    default:
      break;
      // More cases defined in hidsystem/ev_keymap.h
  }

  emit input->receivedInput("AppleMediaKeys", keyPressed, keyIsPressed ? InputBase::KeyDown : InputBase::KeyUp);
}

@end

// macOS private enum
enum {
    MRNowPlayingClientVisibilityUndefined = 0,
    MRNowPlayingClientVisibilityAlwaysVisible,
    MRNowPlayingClientVisibilityVisibleWhenBackgrounded,
    MRNowPlayingClientVisibilityNeverVisible
};

///////////////////////////////////////////////////////////////////////////////////////////////////
bool InputAppleMediaKeys::initInput()
{
  m_currentTime = 0;
  m_pendingUpdate = false;
  m_delegate = [[MediaKeysDelegate alloc] initWithInput:this];
  if (NSClassFromString(@"MPNowPlayingInfoCenter")) {
    connect(&PlayerComponent::Get(), &PlayerComponent::stateChanged, this, &InputAppleMediaKeys::handleStateChanged);
    connect(&PlayerComponent::Get(), &PlayerComponent::positionUpdate, this, &InputAppleMediaKeys::handlePositionUpdate);
    connect(&PlayerComponent::Get(), &PlayerComponent::updateDuration, this, &InputAppleMediaKeys::handleUpdateDuration);
    void* lib = dlopen("/System/Library/PrivateFrameworks/MediaRemote.framework/MediaRemote", RTLD_NOW);
    if (lib) {
#define LOAD_FUNC(name) \
  name = (name ## Func)dlsym(lib, "MRMediaRemote" #name)
      LOAD_FUNC(SetNowPlayingVisibility);
      LOAD_FUNC(GetLocalOrigin);
      LOAD_FUNC(SetCanBeNowPlayingApplication);
      if (SetCanBeNowPlayingApplication)
        SetCanBeNowPlayingApplication(1);
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static MPNowPlayingPlaybackState convertState(PlayerComponent::State newState)
{
  switch (newState) {
    case PlayerComponent::State::finished:
      return MPNowPlayingPlaybackStateStopped;
    case PlayerComponent::State::canceled:
    case PlayerComponent::State::error:
      return MPNowPlayingPlaybackStateInterrupted;
    case PlayerComponent::State::buffering:
    case PlayerComponent::State::paused:
      return MPNowPlayingPlaybackStatePaused;
    case PlayerComponent::State::playing:
      return MPNowPlayingPlaybackStatePlaying;
    default:
      return MPNowPlayingPlaybackStateUnknown;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputAppleMediaKeys::handleStateChanged(PlayerComponent::State newState, PlayerComponent::State oldState)
{
  MPNowPlayingPlaybackState newMPState = convertState(newState);
  MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
  NSMutableDictionary *playingInfo = [NSMutableDictionary dictionaryWithDictionary:center.nowPlayingInfo];
  [playingInfo setObject:[NSNumber numberWithDouble:(double)m_currentTime / 1000] forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  center.nowPlayingInfo = playingInfo;
  [MPNowPlayingInfoCenter defaultCenter].playbackState = newMPState;
  if (SetNowPlayingVisibility && GetLocalOrigin) {
    if (newState == PlayerComponent::State::finished || newState == PlayerComponent::State::canceled || newState == PlayerComponent::State::error)
      SetNowPlayingVisibility(GetLocalOrigin(), MRNowPlayingClientVisibilityNeverVisible);
    else if (newState == PlayerComponent::State::paused || newState == PlayerComponent::State::playing || newState == PlayerComponent::State::buffering)
      SetNowPlayingVisibility(GetLocalOrigin(), MRNowPlayingClientVisibilityAlwaysVisible);
  }

  m_pendingUpdate = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputAppleMediaKeys::handlePositionUpdate(quint64 position)
{
  m_currentTime = position;

  if (m_pendingUpdate) {
    MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
    NSMutableDictionary *playingInfo = [NSMutableDictionary dictionaryWithDictionary:center.nowPlayingInfo];
    [playingInfo setObject:[NSNumber numberWithDouble:(double)position / 1000] forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    center.nowPlayingInfo = playingInfo;
    [MPNowPlayingInfoCenter defaultCenter].playbackState = [MPNowPlayingInfoCenter defaultCenter].playbackState;
    m_pendingUpdate = false;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputAppleMediaKeys::handleUpdateDuration(qint64 duration)
{
  MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
  NSMutableDictionary *playingInfo = [NSMutableDictionary dictionaryWithDictionary:center.nowPlayingInfo];
  [playingInfo setObject:[NSNumber numberWithDouble:(double)duration / 1000] forKey:MPMediaItemPropertyPlaybackDuration];
  center.nowPlayingInfo = playingInfo;
  m_pendingUpdate = true;
}
