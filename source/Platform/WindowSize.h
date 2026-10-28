#pragma once

#include <cstdint>

namespace Platform
{

// A size in pixels.
struct WindowSize
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool operator==(const WindowSize& other) const = default;
};

}
