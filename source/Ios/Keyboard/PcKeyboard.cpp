#include "Ios/Keyboard/PcKeyboard.h"

#include "Ios/Keyboard/PcKeyMap.h"

#include <algorithm>

namespace Ios
{

PcKeyboard::PcKeyboard(KeyboardService& service, Platform::WindowListener& listener)
    : keyboardService(service)
    , nextListener(listener)
{
    keyboardService.ReportConnected(PcKeyboardId);
}

PcKeyboard::~PcKeyboard()
{
    keyboardService.ReportDisconnected(PcKeyboardId);
}

void PcKeyboard::OnMouseMoved(int x, int y)
{
    nextListener.OnMouseMoved(x, y);
}

void PcKeyboard::OnMouseLeft()
{
    nextListener.OnMouseLeft();
}

void PcKeyboard::OnMouseButton(Platform::MouseButton button, bool isPressed)
{
    nextListener.OnMouseButton(button, isPressed);
}

void PcKeyboard::OnKey(int virtualKeyCode, bool isPressed)
{
    nextListener.OnKey(virtualKeyCode, isPressed);
    ChangeKey(virtualKeyCode, isPressed);
}

void PcKeyboard::OnFocusLost()
{
    nextListener.OnFocusLost();

    modifiers = 0;
    pressedKeys.clear();
    ReportKeys();
}

void PcKeyboard::ChangeKey(int virtualKeyCode, bool isPressed)
{
    std::optional<std::uint8_t> modifierBit = UsbModifierOfVirtualKey(virtualKeyCode);
    if (modifierBit)
    {
        ChangeModifier(*modifierBit, isPressed);
        return;
    }

    std::optional<std::uint8_t> usbKey = UsbKeyOfVirtualKey(virtualKeyCode);
    if (usbKey)
        ChangePressedKey(*usbKey, isPressed);
}

void PcKeyboard::ChangeModifier(std::uint8_t modifierBit, bool isPressed)
{
    if (isPressed)
        modifiers = static_cast<std::uint8_t>(modifiers | modifierBit);
    else
        modifiers = static_cast<std::uint8_t>(modifiers & ~modifierBit);

    ReportKeys();
}

void PcKeyboard::ChangePressedKey(std::uint8_t usbKey, bool isPressed)
{
    auto position = std::find(pressedKeys.begin(), pressedKeys.end(), usbKey);
    bool isDownAlready = position != pressedKeys.end();

    if (isPressed && !isDownAlready)
        pressedKeys.push_back(usbKey);

    if (!isPressed && isDownAlready)
        pressedKeys.erase(position);

    ReportKeys();
}

void PcKeyboard::ReportKeys()
{
    keyboardService.ReportKeys(PcKeyboardId, modifiers, pressedKeys);
}

}
