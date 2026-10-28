#pragma once

#include <stdexcept>
#include <string>

namespace Boot
{

// The boot sequence cannot go on. The message says what is wrong in plain words.
class BootError : public std::runtime_error
{
public:
    explicit BootError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
