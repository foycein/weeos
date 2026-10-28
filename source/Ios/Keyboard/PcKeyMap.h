#pragma once

#include <cstdint>
#include <optional>

namespace Ios
{

// Windows names the keys of the PC with virtual key codes. A USB keyboard names them with the codes of UsbKeyCodes.h.
// These two functions turn the first into the second. They give nothing for a key that a USB keyboard does not have.
//
// A virtual key code follows the layout that the user chose in Windows. The code of a USB key follows the place of the key.
// The two agree on a keyboard with the US layout. On another layout a key can get the code of another place.

// The code of a key that is not a modifier. The keypad Enter key cannot be told from the Enter key by its virtual key code.
std::optional<std::uint8_t> UsbKeyOfVirtualKey(int virtualKeyCode);

// The bit of the modifier byte (UsbModifier) for a control, shift, alt or windows key.
// The window reports the control, shift and alt keys without a side. They count as the left keys.
std::optional<std::uint8_t> UsbModifierOfVirtualKey(int virtualKeyCode);

}
