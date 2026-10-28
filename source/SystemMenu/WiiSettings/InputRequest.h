#pragma once

#include <cstddef>
#include <string>

namespace SystemMenu
{

// What a page needs from the user that the page cannot show itself: some text from the soft keyboard, a number, or the parental password.
// The page waits. The screen shows the keyboard or the dialog and then tells the flow what the user did.
enum class InputKind
{
    Text,

    // Digits only, for a PIN or an address.
    Digits,

    // The dialog of the parental controls. It checks the PIN itself, so the answer is only "accepted" or "cancelled".
    ParentalPassword,
};

struct InputRequest
{
    InputKind kind = InputKind::Text;

    // The text that is in the field when the keyboard opens.
    std::u16string initialText;

    std::size_t maximumLength = 0;
};

}
