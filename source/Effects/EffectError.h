#pragma once

#include <stdexcept>
#include <string>

namespace Effects
{

// Thrown when an effect cannot be made: its name is not in the loaded files, or it asks for a part of the effect library that WeeOS
// does not have because no channel shows it without WeeOS's own server.
class EffectError : public std::runtime_error
{
public:
    explicit EffectError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
