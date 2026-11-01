#pragma once

#include <stdexcept>
#include <string>

namespace Channels
{

// Thrown when a channel cannot go on: a content of its title or a file inside one is missing, or a pane that a screen needs
// is not in its layout.
class ChannelError : public std::runtime_error
{
public:
    explicit ChannelError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
