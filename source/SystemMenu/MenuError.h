#pragma once

#include <stdexcept>
#include <string>

namespace SystemMenu
{

// Thrown when the menu cannot go on: a file of the System Menu title is missing, or a pane that a screen needs is not in its layout.
class MenuError : public std::runtime_error
{
public:
    explicit MenuError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
