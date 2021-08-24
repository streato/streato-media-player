set(MAIN_TARGET StreatoMediaPlayer)

# Name of the output binary, defaults are only used on Linux
set(MAIN_NAME jellyfinmediaplayer)

if(APPLE)
  set(MAIN_NAME "Streato Media Player")
elseif(WIN32)
  set(MAIN_NAME "StreatoMediaPlayer")
endif(APPLE)

configure_file(src/shared/Names.cpp.in src/shared/Names.cpp @ONLY)