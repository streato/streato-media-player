
#include "QsLog.h"
#include "InputCEC.h"
#include "settings/SettingsComponent.h"
#include "power/PowerComponent.h"

struct KeyAction
{
  QString action;
  bool hasLongPress;
};

static QMap<int, KeyAction> g_cecKeyMap   { \
                                        { CEC_USER_CONTROL_CODE_SELECT , { INPUT_KEY_SELECT , false } } , \
                                        { CEC_USER_CONTROL_CODE_UP , { INPUT_KEY_UP , false } } , \
                                        { CEC_USER_CONTROL_CODE_DOWN , { INPUT_KEY_DOWN , false } } , \
                                        { CEC_USER_CONTROL_CODE_LEFT , { INPUT_KEY_LEFT , false } } , \
                                        { CEC_USER_CONTROL_CODE_RIGHT , { INPUT_KEY_RIGHT , false } } , \
                                        { CEC_USER_CONTROL_CODE_SETUP_MENU , { INPUT_KEY_MENU , false } } , \
                                        { CEC_USER_CONTROL_CODE_PLAY , { INPUT_KEY_PLAY , false } } , \
                                        { CEC_USER_CONTROL_CODE_PAUSE , { INPUT_KEY_PAUSE , false } } , \
                                        { CEC_USER_CONTROL_CODE_STOP , { INPUT_KEY_STOP , false } } , \
                                        { CEC_USER_CONTROL_CODE_EXIT , { INPUT_KEY_BACK , false } } , \
                                        { CEC_USER_CONTROL_CODE_FAST_FORWARD , { INPUT_KEY_SEEKFWD , false } } , \
                                        { CEC_USER_CONTROL_CODE_REWIND , { INPUT_KEY_SEEKBCK , false } } , \
                                        { CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION , { INPUT_KEY_INFO , false } } , \
                                        { CEC_USER_CONTROL_CODE_FORWARD , { INPUT_KEY_NEXT , false } } , \
                                        { CEC_USER_CONTROL_CODE_BACKWARD , { INPUT_KEY_PREV , false } } , \
                                        { CEC_USER_CONTROL_CODE_F1_BLUE , { INPUT_KEY_BLUE , false } } , \
                                        { CEC_USER_CONTROL_CODE_F2_RED , { INPUT_KEY_RED , false } } , \
                                        { CEC_USER_CONTROL_CODE_F3_GREEN , { INPUT_KEY_GREEN , false } } , \
                                        { CEC_USER_CONTROL_CODE_F4_YELLOW , { INPUT_KEY_YELLOW , false } } , \
                                        { CEC_USER_CONTROL_CODE_SUB_PICTURE, { INPUT_KEY_SUBTITLES , false } } , \
                                        { CEC_USER_CONTROL_CODE_ROOT_MENU, { INPUT_KEY_HOME , false } }, \
                                        { CEC_USER_CONTROL_CODE_NUMBER0, { INPUT_KEY_0 , false } } ,  \
                                        { CEC_USER_CONTROL_CODE_NUMBER1, { INPUT_KEY_1 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER2, { INPUT_KEY_2 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER3, { INPUT_KEY_3 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER4, { INPUT_KEY_4 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER5, { INPUT_KEY_5 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER6, { INPUT_KEY_6 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER7, { INPUT_KEY_7 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER8, { INPUT_KEY_8 , false } } , \
                                        { CEC_USER_CONTROL_CODE_NUMBER9, { INPUT_KEY_9 , false } } , \
                                        { CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE, { INPUT_KEY_GUIDE , false } } \
                                        };

//////////////////////////////////////////////////////////////////////////////////////////////////
InputCEC::InputCEC(QObject *parent) : InputBase(parent)
{
  m_cecThread = new QThread(this);
  m_cecThread->setObjectName("InputCEC");

  m_cecWorker = new InputCECWorker(nullptr);
  m_cecWorker->moveToThread(m_cecThread);

  m_cecThread->start(QThread::LowPriority);
  connect(m_cecWorker, &InputCECWorker::receivedInput, this, &InputCEC::receivedInput);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
bool InputCEC::initInput()
{
  bool retVal;
  QMetaObject::invokeMethod(m_cecWorker, "init", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, retVal));

  return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////////
InputCEC::~InputCEC()
{
  QMetaObject::invokeMethod(m_cecWorker, "closeCec", Qt::BlockingQueuedConnection);

  m_cecThread->exit(0);
  m_cecThread->wait();

  delete m_cecWorker;
}

/////////////////////////////////////////////////////////////////////////////////////////
bool InputCECWorker::init()
{
  m_configuration.Clear();
  m_callbacks.Clear();

  m_verboseLogging = SettingsComponent::Get().value(SETTINGS_SECTION_CEC, "verbose_logging").toBool();

  m_configuration.clientVersion = LIBCEC_VERSION_CURRENT;
  qstrcpy(m_configuration.strDeviceName, "Streato");
  m_configuration.bActivateSource = 0;
  m_callbacks.logMessage = &CecLogMessage;
  m_callbacks.commandReceived = &CecCommand;
  m_callbacks.alert = &CecAlert;
  m_configuration.callbackParam = this;
  m_configuration.callbacks = &m_callbacks;
  m_configuration.deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);
  m_configuration.bAutodetectAddress =  CEC_DEFAULT_SETTING_AUTODETECT_ADDRESS;
  m_configuration.iPhysicalAddress = CEC_PHYSICAL_ADDRESS_TV;
  m_configuration.baseDevice = CECDEVICE_AUDIOSYSTEM;
  m_configuration.bActivateSource = (uint8_t)SettingsComponent::Get().value(SETTINGS_SECTION_CEC, "activatesource").toBool();
  m_configuration.iHDMIPort = (quint8)SettingsComponent::Get().value(SETTINGS_SECTION_CEC, "hdmiport").toInt();

  // open libcec
  m_adapter = (ICECAdapter*)CECInitialise(&m_configuration);
  if (!m_adapter)
  {
    QLOG_ERROR() << "Unable to initialize libCEC.";
    return false;
  }

  QLOG_INFO() << "libCEC was successfully initialized, found version"
              << m_configuration.serverVersion;

  // init video on targets that need this
  m_adapter->InitVideoStandalone();

  // check for attached adapters
  checkAdapter();

  // Start a timer to keep track of attached/removed adapters
  m_timer = new QTimer(nullptr);
  m_timer->setInterval(10 * 1000);
  connect(m_timer, &QTimer::timeout, this, &InputCECWorker::checkAdapter);
  m_timer->start();

  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::closeCec()
{
  if (m_timer->isActive())
  {
    m_timer->stop();
    delete m_timer;
  }

  if (m_adapter)
  {
    QLOG_DEBUG() << "Closing libCEC.";
    closeAdapter();
    CECDestroy(m_adapter);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
bool InputCECWorker::openAdapter()
{
  bool ret = false;

  // try to find devices
  cec_adapter_descriptor devices[10];
  int devicesCount = m_adapter->DetectAdapters(devices, 10, nullptr, false);
  if (devicesCount > 0)
  {
    // list devices
    QLOG_INFO() << "libCEC found" << devicesCount << "CEC adapters.";

    // open first adapter
    m_adapterPort = devices[0].strComName;
    if (m_adapter->Open(m_adapterPort.toStdString().c_str()))
    {
      QLOG_INFO() << "Device " << devices[0].strComName << "was successfully openned";
      ret = true;
    }
    else
    {
      QLOG_ERROR() << "Opening device" << devices[0].strComName << "failed";
      ret = false;
    }
  }

  return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::closeAdapter()
{
  m_adapterPort.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::checkAdapter()
{
  if (m_adapterPort.isEmpty())
  {    
    if (m_adapter)
      m_adapter->Close();

    openAdapter();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::sendReceivedInput(const QString &source, const QString &keycode, InputBase::InputkeyState keyState)
{
  emit receivedInput(source, keycode, keyState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
QString InputCECWorker::getCommandString(cec_user_control_code code)
{
  QString key;

  if (g_cecKeyMap.contains(code))
  {
    KeyAction keyaction = g_cecKeyMap[code];
    key = keyaction.action;
  }

  return key;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::CecLogMessage(void* cbParam, const cec_log_message *message)
{
  auto *cec = static_cast<InputCECWorker*>(cbParam);

  Q_ASSERT(cec);

  switch (message->level)
  {
    case CEC_LOG_ERROR:
      QLOG_ERROR() << "libCEC ERROR:" << message->message;
      break;

    case CEC_LOG_WARNING:
      QLOG_WARN() << "libCEC WARNING:" << message->message;
      break;

    case CEC_LOG_NOTICE:
      QLOG_INFO() << "libCEC NOTICE:" << message->message;
      break;

    case CEC_LOG_DEBUG:
      if (cec->m_verboseLogging)
      {
        QLOG_DEBUG() << "libCEC DEBUG:" << message->message;
      }
      break;

    case CEC_LOG_TRAFFIC:
      break;

    default:
      break;
  }

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
QString InputCECWorker::getCommandParamsList(const cec_command *command)
{
  QString output = QString("%1 parameter(s) :").arg(command->parameters.size);

  if (command->parameters.size)
  {
    for (int i=0; i<command->parameters.size; i++)
      output += QString("[%1]=%2").arg(i).arg(QString::number(command->parameters[i], 16).toUpper());
  }

  return output;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::CecCommand(void *cbParam, const cec_command *command)
{
  QString cmdString, keyCode;
  bool useUpDown = SettingsComponent::Get().value(SETTINGS_SECTION_CEC, "usekeyupdown").toBool();
  auto cec = static_cast<InputCECWorker*>(cbParam);
  Q_ASSERT(cec);

  if (cec->m_verboseLogging)
  {
    QLOG_DEBUG() << "CecCommand received " << QString::number(command->opcode, 16).toUpper() << "," << cec->getCommandParamsList(command);
  }

  switch(command->opcode)
  {
    case CEC_OPCODE_PLAY:
      cec->sendReceivedInput(CEC_INPUT_NAME, INPUT_KEY_PLAY, InputBase::KeyPressed);
      break;

    case CEC_OPCODE_DECK_CONTROL:
      if (command->parameters.size)
      {
        switch(command->parameters[0])
        {
          case CEC_DECK_CONTROL_MODE_SKIP_FORWARD_WIND:
            keyCode = INPUT_KEY_SEEKFWD;
            break;

          case CEC_DECK_CONTROL_MODE_SKIP_REVERSE_REWIND:
            keyCode = INPUT_KEY_SEEKBCK;
            break;

          case CEC_DECK_CONTROL_MODE_STOP:
            keyCode = INPUT_KEY_STOP;
            break;

          default:
            break;
        }

        if (!keyCode.isEmpty())
        {
          // We don't have up & down events for those special keys
          // so we just fake them
          cec->sendReceivedInput(CEC_INPUT_NAME, keyCode, InputBase::KeyPressed);
        }
      }
      break;

    case CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN:
    case CEC_OPCODE_USER_CONTROL_PRESSED:
    case CEC_OPCODE_USER_CONTROL_RELEASE:
    case CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP:
    {
      bool down = (command->opcode == CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN) ||
                  (command->opcode == CEC_OPCODE_USER_CONTROL_PRESSED);


      if (cec->m_verboseLogging)
      {
        QLOG_DEBUG() << "CecCommand button (Down= " << down << ")" << cec->getCommandParamsList(command);
      }

      if (command->parameters.size && down)
      {
        switch(command->parameters[0])
        {
          // samsung Return key
          case CEC_USER_CONTROL_CODE_AN_RETURN:
            if (useUpDown)
              cec->sendReceivedInput(CEC_INPUT_NAME, INPUT_KEY_BACK, down ? InputBase::KeyDown : InputBase::KeyUp);
            else if (down)
              cec->sendReceivedInput(CEC_INPUT_NAME, INPUT_KEY_BACK, InputBase::KeyPressed);

            return;
            break;

          default:
            break;
        }
      }

      cmdString = cec->getCommandString((cec_user_control_code)command->parameters[0]);

      if (!cmdString.isEmpty())
      {
        if (useUpDown)
          cec->sendReceivedInput(CEC_INPUT_NAME, cmdString, down ? InputBase::KeyDown : InputBase::KeyUp);
        else if (down)
          cec->sendReceivedInput(CEC_INPUT_NAME, cmdString, InputBase::KeyPressed);
      }
    }
      break;

    case CEC_OPCODE_GIVE_OSD_NAME:          // ignore those known commands (only pollng from TV)
    case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
      break;

    case CEC_OPCODE_STANDBY:
      QLOG_DEBUG() << "CecCommand : Got a standby Request";
      if ((SettingsComponent::Get().value(SETTINGS_SECTION_CEC, "suspendonstandby").toBool()) && PowerComponent::Get().canSuspend())
      {
        PowerComponent::Get().Suspend();
      }
      else if ((SettingsComponent::Get().value(SETTINGS_SECTION_CEC, "poweroffonstandby").toBool()) && PowerComponent::Get().canPowerOff())
      {
        PowerComponent::Get().PowerOff();
      }
      break;

    default:
      QLOG_DEBUG() << "Unhandled CEC command " << command->opcode << ", " << cec->getCommandParamsList(command);
      break;
  }

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void InputCECWorker::CecAlert(void *cbParam, const libcec_alert type, const libcec_parameter param)
{
  bool reopen = false;

  switch (type)
  {
    case CEC_ALERT_SERVICE_DEVICE:
      QLOG_ERROR() << "libCEC : Alert CEC_ALERT_SERVICE_DEVICE";
      break;

    case CEC_ALERT_CONNECTION_LOST:
      QLOG_ERROR() << "libCEC : Alert CEC_ALERT_CONNECTION_LOST";
      reopen = true;
      break;

    case CEC_ALERT_PERMISSION_ERROR:
      QLOG_ERROR() << "libCEC : Alert CEC_ALERT_PERMISSION_ERROR";
      reopen = true;
      break;

    case CEC_ALERT_PORT_BUSY:
      QLOG_ERROR() << "libCEC : Alert CEC_ALERT_PORT_BUSY";
      reopen = true;
      break;

    default:
      break;
  }

  if (reopen)
  {
    QLOG_DEBUG() << "libCEC : Reopenning adapter";
    auto cec = static_cast<InputCECWorker*>(cbParam);
    if (cec)
      cec->closeAdapter();
  }

  return;
}
