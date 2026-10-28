#pragma once

#include <stdexcept>
#include <string>

namespace Platform
{

// Thrown when Windows refuses something the window or the clock needs.
class PlatformError : public std::runtime_error
{
public:
    explicit PlatformError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
