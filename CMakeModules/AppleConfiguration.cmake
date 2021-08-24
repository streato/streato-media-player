find_library(FOUNDATION Foundation)
find_library(APPKIT AppKit)
find_library(IOKIT IOKit)
find_library(COCOA Cocoa)
find_Library(CARBON Carbon)
find_library(SECURITY Security)
find_library(MEDIAPLAYER MediaPlayer)

set(OS_LIBS ${FOUNDATION} ${APPKIT} ${IOKIT} ${COCOA} ${SECURITY} ${CARBON} spmediakeytap hidremote plistparser letsmove)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -weak_framework MediaPlayer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.9 -fno-omit-frame-pointer")
set(WARNINGS "-Wall")

set(HAVE_UPDATER 1)
find_program(UPDATER_PATH updater HINTS ${CMAKE_FIND_ROOT_PATH}/update_installer/ NO_DEFAULT_PATH)
if(${UPDATER_PATH} MATCHES "UPDATER_PATH-NOTFOUND")
  set(HAVE_UPDATER 0)
  message(STATUS "will build without the updater")
endif(${UPDATER_PATH} MATCHES "UPDATER_PATH-NOTFOUND")

set(INSTALL_BIN_DIR .)
set(INSTALL_RESOURCE_DIR Resources)