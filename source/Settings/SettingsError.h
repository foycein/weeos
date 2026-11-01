#pragma once

#include <stdexcept>
#include <string>

namespace Settings
{

// Thrown when the settings of the console cannot be read or written.
class SettingsError : public std::runtime_error
{
public:
    explicit SettingsError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
