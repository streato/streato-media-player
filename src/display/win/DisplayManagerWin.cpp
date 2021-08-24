//
//  DisplayManagerWin.cpp
//  konvergo
//
//  Created by Lionel CHAZALLON on 18/06/2015.
//
//

#include <QRect>
#include <math.h>

#include "QsLog.h"
#include "DisplayManagerWin.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
static DMVideoMode convertDevMode(const DEVMODEW& devmode)
{
  DMVideoMode mode = {};
  mode.m_height = devmode.dmPelsHeight;
  mode.m_width = devmode.dmPelsWidth;
  mode.m_refreshRate = devmode.dmDisplayFrequency;
  mode.m_bitsPerPixel = devmode.dmBitsPerPel;
  mode.m_interlaced = !!(devmode.dmDisplayFlags & DM_INTERLACED);

  // Windows just returns integer refresh rate so let's fudge it
  if (mode.m_refreshRate == 59 ||
      mode.m_refreshRate == 29 ||
      mode.m_refreshRate == 23)
      mode.m_refreshRate = (float)(mode.m_refreshRate + 1) / 1.001f;

  return mode;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static bool modeEquals(const DMVideoMode& m1, const DMVideoMode& m2)
{
  return m1.m_height == m2.m_height &&
         m1.m_width == m2.m_width &&
         fabs(m1.m_refreshRate - m2.m_refreshRate) < 1e-9 &&
         m1.m_bitsPerPixel == m2.m_bitsPerPixel &&
         m1.m_interlaced == m2.m_interlaced;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool DisplayManagerWin::initialize()
{
  DISPLAY_DEVICEW displayInfo;
  int displayId = 0;

  m_displayAdapters.clear();
  m_displays.clear();

  while (getDisplayInfo(displayId, displayInfo))
  {
    if (displayInfo.StateFlags & (DISPLAY_DEVICE_ACTIVE | DISPLAY_DEVICE_ATTACHED))
    {
      DEVMODEW modeInfo;
      int modeId = 0;

      // add the display
      DMDisplayPtr display = DMDisplayPtr(new DMDisplay);
      display->m_id = displayId;
      display->m_name = QString::fromWCharArray(displayInfo.DeviceString);
      m_displays[display->m_id] = DMDisplayPtr(display);
      m_displayAdapters[display->m_id] = QString::fromWCharArray(displayInfo.DeviceName);

      while (getModeInfo(displayId, modeId, modeInfo))
      {
        // add the videomode to the display
        DMVideoModePtr videoMode = DMVideoModePtr(new DMVideoMode);
        *videoMode = convertDevMode(modeInfo);
        videoMode->m_id = modeId;
        display->m_videoModes[videoMode->m_id] = videoMode;

        modeId++;
      }
    }

    displayId++;
  }

  if (m_displays.isEmpty())
  {
    QLOG_DEBUG() << "No display found.";
    return false;
  }
  else
    return DisplayManager::initialize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool DisplayManagerWin::setDisplayMode(int display, int mode)
{
  DEVMODEW modeInfo;

  if (!isValidDisplayMode(display, mode))
    return false;

  if (getModeInfo(display, mode, modeInfo))
  {
    QLOG_DEBUG() << "Switching to mode" << mode << "on display" << display << ":" << m_displays[display]->m_videoModes[mode]->getPrettyName();

    modeInfo.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;

    LONG rc = ChangeDisplaySettingsExW((LPCWSTR)m_displayAdapters[display].utf16(), &modeInfo, NULL,
                                       CDS_FULLSCREEN, NULL);

    if (rc != DISP_CHANGE_SUCCESSFUL)
    {
      QLOG_ERROR() << "Failed to changed DisplayMode, error" << rc;
      return false;
    }
    else
    {
      return true;
    }
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int DisplayManagerWin::getCurrentDisplayMode(int display)
{
  if (!isValidDisplay(display))
    return -1;

  // grab current mode
  DEVMODEW modeInfo;
  ZeroMemory(&modeInfo, sizeof(modeInfo));
  modeInfo.dmSize = sizeof(modeInfo);

  // grab current mode info
  if (!EnumDisplaySettingsW((LPCWSTR)m_displayAdapters[display].utf16(), ENUM_CURRENT_SETTINGS,
                            &modeInfo))
  {
    QLOG_ERROR() << "Failed to retrieve current mode";
    return -1;
  }

  DMVideoMode mode = convertDevMode(modeInfo);

  // check if current mode info matches on of our modes
  for (int modeId = 0; modeId < m_displays[display]->m_videoModes.size(); modeId++)
  {
    if (modeEquals(mode, * m_displays[display]->m_videoModes[modeId]))
      return modeId;
  }

  return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int DisplayManagerWin::getMainDisplay()
{
  DISPLAY_DEVICEW displayInfo;
  int displayId = 0;

  while (getDisplayInfo(displayId, displayInfo))
  {
    if (displayInfo.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
      return displayId;

    displayId++;
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
DisplayManagerWin::~DisplayManagerWin()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int DisplayManagerWin::getDisplayFromPoint(int x, int y)
{
  for(int displayId : m_displays.keys())
  {
    QString dispName = m_displayAdapters[displayId];

    DEVMODEW modeInfo = {};
    modeInfo.dmSize = sizeof(modeInfo);

    QLOG_TRACE() << "Looking at display" << displayId << dispName;

    if (!EnumDisplaySettingsW((LPCWSTR)dispName.utf16(), ENUM_CURRENT_SETTINGS,
                              &modeInfo))
    {
      QLOG_ERROR() << "Failed to retrieve current mode.";
    }
    else
    {
      QRect displayRect(modeInfo.dmPosition.x, modeInfo.dmPosition.y, modeInfo.dmPelsWidth,
                        modeInfo.dmPelsHeight);
      QLOG_TRACE() << "Position on virtual desktop:" << displayRect;

      if (displayRect.contains(x, y))
        return displayId;
    }
  }

  return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool DisplayManagerWin::getDisplayInfo(int display, DISPLAY_DEVICEW& info)
{
  ZeroMemory(&info, sizeof(info));
  info.cb = sizeof(info);
  return EnumDisplayDevicesW(NULL, display, &info, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool DisplayManagerWin::getModeInfo(int display, int mode, DEVMODEW& info)
{
  if (m_displayAdapters.contains(display))
  {
    ZeroMemory(&info, sizeof(info));
    info.dmSize = sizeof(info);

    return EnumDisplaySettingsExW((LPCWSTR)m_displayAdapters[display].utf16(), mode, &info, 0);
  }

  return false;
}
