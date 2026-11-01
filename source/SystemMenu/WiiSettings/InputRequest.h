#pragma once

#include <cstddef>
#include <string>

namespace SystemMenu
{

// Text that a page needs from the user and cannot ask for itself. The page waits; the screen shows the soft keyboard and then tells the
// flow what the user typed.
struct InputRequest
{
    // The words the keyboard shows above its field.
    std::u16string title;

    // The text that is in the field when the keyboard opens.
    std::u16string initialText;

    std::size_t maximumLength = 0;
};

}
