#pragma once

#include <stdexcept>
#include <string>

namespace Text
{

// Thrown when a text cannot be made: a message that does not exist, a format that does not fit its arguments, a font without a size.
class TextError : public std::runtime_error
{
public:
    explicit TextError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
