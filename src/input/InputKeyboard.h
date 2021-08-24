//
// Created by Tobias Hieta on 09/06/15.
//

#ifndef KONVERGO_INPUTKEYBOARD_H
#define KONVERGO_INPUTKEYBOARD_H

#include <QKeySequence>
#include "InputComponent.h"
#include "QsLog.h"

class InputKeyboard : public InputBase
{
  Q_OBJECT
  DEFINE_SINGLETON(InputKeyboard);

public:
  bool initInput() override { return true; }
  const char* inputName() override { return "Keyboard"; }

  void keyPress(const QString& keys, InputkeyState keyState)
  {
    emit receivedInput("Keyboard", keys, keyState);
  }

private:
  explicit InputKeyboard(QObject* parent = nullptr) : InputBase(parent) {}
};

#endif //KONVERGO_INPUTKEYBOARD_H
