#pragma once

#include <stdexcept>
#include <string>

namespace Layout
{

// Thrown when a layout cannot be built, animated or drawn: a file is missing, or the layout uses something the library does not support.
class LayoutError : public std::runtime_error
{
public:
    explicit LayoutError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
