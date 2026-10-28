#pragma once

#include <cstdint>

namespace Platform
{

// The shape the picture has on the screen. The window shows it in this shape, with black bars where the window is wider or taller.
enum class ScreenShape : std::uint8_t
{
    FourByThree,
    SixteenByNine,
};

}
