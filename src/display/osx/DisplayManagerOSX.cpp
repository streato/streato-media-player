//
//  DisplayManagerOSX.cpp
//  konvergo
//
//  Created by Lionel CHAZALLON on 28/09/2014.
//
//

#include <CoreGraphics/CoreGraphics.h>
#include "utils/osx/OSXUtils.h"
#include "DisplayManagerOSX.h"

#include "QsLog.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
bool DisplayManagerOSX::initialize()
{
  int totalModes = 0;

  m_displays.clear();

  for (int i = 0; i < m_osxDisplayModes.size(); i++)
  {
    if (m_osxDisplayModes[i])
      CFRelease(m_osxDisplayModes[i]);
  }
  m_osxDisplayModes.clear();

  CGError err = CGGetActiveDisplayList(MAX_DISPLAYS, m_osxDisplays, &m_osxnumDisplays);
  if (err)
  {
    m_osxnumDisplays = 0;
    QLOG_ERROR() << "CGGetActiveDisplayList returned failure:" << err;
    return false;
  }

  for (int displayid = 0; displayid < m_osxnumDisplays; displayid++)
  {
    // add the display to the list
    DMDisplayPtr display = DMDisplayPtr(new DMDisplay);
    display->m_id = displayid;
    display->m_name = QString("Display %1").arg(displayid);
    m_displays[display->m_id] = display;

    m_osxDisplayModes[displayid] = CGDisplayCopyAllDisplayModes(m_osxDisplays[displayid], nullptr);
    if (!m_osxDisplayModes[displayid])
      continue;

    int numModes = (int)CFArrayGetCount(m_osxDisplayModes[displayid]);

    for (int modeid = 0; modeid < numModes; modeid++)
    {
      totalModes++;
      
      // add the videomode to the display
      DMVideoModePtr mode = DMVideoModePtr(new DMVideoMode);
      mode->m_id = modeid;
      display->m_videoModes[modeid] = mode;

      // grab videomode info
      CGDisplayModeRef displayMode =
      (CGDisplayModeRef)CFArrayGetValueAtIndex(m_osxDisplayModes[displayid], modeid);

      mode->m_height = (int)CGDisplayModeGetHeight(displayMode);
      mode->m_width = (int)CGDisplayModeGetWidth(displayMode);
      mode->m_refreshRate = (float)CGDisplayModeGetRefreshRate(displayMode);

      CFStringRef pixEnc = CGDisplayModeCopyPixelEncoding(displayMode);

      if (CFStringCompare(pixEnc, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        mode->m_bitsPerPixel = 32;
      else if (CFStringCompare(pixEnc, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        mode->m_bitsPerPixel = 16;
      else if (CFStringCompare(pixEnc, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        mode->m_bitsPerPixel = 8;

      CFRelease(pixEnc);

      mode->m_interlaced = (CGDisplayModeGetIOFlags(displayMode) & kDisplayModeInterlacedFlag) > 0;

      if (mode->m_refreshRate == 0)
        mode->m_refreshRate = 60;
    }
  }

  if (totalModes == 0)
    return false;
  else
    return DisplayManager::initialize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool DisplayManagerOSX::setDisplayMode(int display, int mode)
{
  if (!isValidDisplayMode(display, mode) || !m_osxDisplayModes[display])
    return false;
  
  CGDisplayModeRef displayMode =
  (CGDisplayModeRef)CFArrayGetValueAtIndex(m_osxDisplayModes[display], mode);

  CGError err = CGDisplaySetDisplayMode(m_osxDisplays[display], displayMode, nullptr);
  if (err)
  {
    QLOG_ERROR() << "CGDisplaySetDisplayMode() returned failure:" << err;
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int DisplayManagerOSX::getCurrentDisplayMode(int display)
{
  if (!isValidDisplay(display) || !m_osxDisplayModes[display])
    return -1;
  
  CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(m_osxDisplays[display]);
  uint32_t currentIOKitID = CGDisplayModeGetIODisplayModeID(currentMode);

  for (int mode = 0; mode < CFArrayGetCount(m_osxDisplayModes[display]); mode++)
  {
    CGDisplayModeRef checkMode = (CGDisplayModeRef)CFArrayGetValueAtIndex(m_osxDisplayModes[display], mode);
    uint32_t checkIOKitID = CGDisplayModeGetIODisplayModeID(checkMode);

    if (currentIOKitID == checkIOKitID)
    {
      CFRelease(currentMode);
      return mode;
    }
  }
  CFRelease(currentMode);

  return -1;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
int DisplayManagerOSX::getMainDisplay()
{
  CGDirectDisplayID mainID = CGMainDisplayID();

  for (int i = 0; i < m_osxnumDisplays; i++)
  {
    if (m_osxDisplays[i] == mainID)
      return i;
  }

  return -1;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
DisplayManagerOSX::~DisplayManagerOSX()
{
  for (int i = 0; i < m_osxDisplayModes.size(); i++)
  {
    if (m_osxDisplayModes[i])
      CFRelease(m_osxDisplayModes[i]);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int DisplayManagerOSX::getDisplayFromPoint(int x, int y)
{
  CGPoint point = { (double)x, (double)y };
  CGDirectDisplayID foundDisplay;
  uint32_t numFound;

  CGGetDisplaysWithPoint(point, 1, &foundDisplay, &numFound);

  for (int i=0; i<m_osxnumDisplays; i++)
  {
    if (foundDisplay == m_osxDisplays[i])
      return i;
  }

  return -1;
}
