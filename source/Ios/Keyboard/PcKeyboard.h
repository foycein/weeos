#pragma once

#include "Ios/Keyboard/KeyboardService.h"

#include "Platform/WindowListener.h"

#include <cstdint>
#include <vector>

namespace Ios
{

// The id of the keyboard of the PC in the keyboard service.
constexpr std::uint32_t PcKeyboardId = 1;

// The keyboard of the PC as a USB keyboard. It is the listener of the window: it takes the keys of the window and reports them to the
// keyboard service the way a USB keyboard reports them. Everything the window tells goes on to the listener behind it, which is the
// virtual remote, because a window has only one listener.
//
// The keyboard is attached as long as this object exists. Make it after IosServices::Start and destroy it before Stop.
// Like every window listener it is used from the thread of the window only.
class PcKeyboard : public Platform::WindowListener
{
public:
    PcKeyboard(KeyboardService& service, Platform::WindowListener& listener);
    ~PcKeyboard() override;

    PcKeyboard(const PcKeyboard&) = delete;
    PcKeyboard& operator=(const PcKeyboard&) = delete;

    void OnMouseMoved(int x, int y) override;
    void OnMouseLeft() override;
    void OnMouseButton(Platform::MouseButton button, bool isPressed) override;
    void OnMouseWheel(int notches) override;

    // The window does not repeat keys. A key that is down already stays as it is.
    void OnKey(int virtualKeyCode, bool isPressed) override;

    // The window does not report the keys that are let go after this, so they all count as let go.
    void OnFocusLost() override;

private:
    void ChangeKey(int virtualKeyCode, bool isPressed);
    void ChangeModifier(std::uint8_t modifierBit, bool isPressed);
    void ChangePressedKey(std::uint8_t usbKey, bool isPressed);
    void ReportKeys();

    KeyboardService& keyboardService;
    Platform::WindowListener& nextListener;

    std::uint8_t modifiers = 0;

    // The keys that are down, the one that went down first at the front.
    std::vector<std::uint8_t> pressedKeys;
};

}
