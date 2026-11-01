#pragma once

#include <string_view>

namespace Platform
{

// A plain message box of Windows with the error sign and an OK button, for a program that cannot start. The texts are UTF-8.
// Waits until the person closes it.
void ShowErrorMessage(std::string_view title, std::string_view text);

}
