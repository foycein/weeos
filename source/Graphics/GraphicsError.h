#pragma once

#include <stdexcept>
#include <string>

namespace Graphics
{

// Thrown when the graphics device cannot do what it was asked: a Vulkan call failed, or a rule of the interface was broken.
class GraphicsError : public std::runtime_error
{
public:
    explicit GraphicsError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
