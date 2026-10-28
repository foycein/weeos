#pragma once

#include <stdexcept>
#include <string>

namespace Input
{

// Thrown when the program asks the input part for something that does not exist, like a fifth remote.
class InputError : public std::runtime_error
{
public:
    explicit InputError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
