#include "Platform/KeyCodes.h"

namespace Platform
{

namespace
{

// Both shift keys send the code of the shift key without an extended mark, so only their scan codes tell them apart.
constexpr unsigned int RightShiftScanCode = 0x36;

}

int ReportedKeyCode(int virtualKeyCode, unsigned int scanCode, bool isExtendedKey)
{
    if (virtualKeyCode == KeyCode::Shift)
    {
        if (scanCode == RightShiftScanCode)
            return KeyCode::RightShift;

        return KeyCode::LeftShift;
    }

    if (virtualKeyCode == KeyCode::Control)
    {
        if (isExtendedKey)
            return KeyCode::RightControl;

        return KeyCode::LeftControl;
    }

    if (virtualKeyCode == KeyCode::Alt)
    {
        if (isExtendedKey)
            return KeyCode::RightAlt;

        return KeyCode::LeftAlt;
    }

    if (virtualKeyCode == KeyCode::Enter && isExtendedKey)
        return KeyCode::KeypadEnter;

    return virtualKeyCode;
}

}
