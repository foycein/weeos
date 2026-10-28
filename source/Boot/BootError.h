#pragma once

#include "Ios/IosError.h"

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

    // A service of IOS refused. The message ends with the result code that the original IOS returns for the same cause.
    BootError(const std::string& message, Ios::IosError error)
        : std::runtime_error(message + " (result code " + std::to_string(Ios::OriginalResultCode(error)) + ")")
    {
    }
};

}
