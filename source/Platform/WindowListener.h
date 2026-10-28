#pragma once

namespace Platform
{

enum class MouseButton
{
    Left,
    Middle,
    Right,
};

// Receives what the user does to a window, in the order it happens. The Input part implements this.
class WindowListener
{
public:
    virtual ~WindowListener() = default;

    // The mouse moved over the window. The position is in pixels from the top left corner of the area inside the window frame.
    virtual void OnMouseMoved(int x, int y) = 0;

    // The mouse left the area inside the window frame.
    virtual void OnMouseLeft() = 0;

    virtual void OnMouseButton(MouseButton button, bool isPressed) = 0;

    // A key went down or up. The code is the Windows virtual key code, with the side of the shift, control and alt keys and a code
    // of its own for the Enter key of the keypad (KeyCodes.h). Holding a key down gives one event, not many.
    virtual void OnKey(int virtualKeyCode, bool isPressed) = 0;

    // The window lost the keyboard focus. A key that was held down is not reported as released after this.
    virtual void OnFocusLost() = 0;
};

}
