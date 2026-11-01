#pragma once

namespace SystemMenu
{

// How the password dialog of the parental controls ended (screen 0x1b). The screen that opened the dialog reads the answer when the dialog is gone.
enum class ParentalAnswer
{
    // The dialog did not end yet, or none was opened.
    None,

    PinCorrect,

    // Three wrong PINs.
    TooManyWrongPins,

    // The user left with the Quit button.
    Cancelled,
};

}
