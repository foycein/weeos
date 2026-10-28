#pragma once

namespace Platform
{

// A rectangle of pixels. The origin is the top left corner of the target, x grows to the right and y grows downward.
struct PixelRectangle
{
    float left = 0;
    float top = 0;
    float width = 0;
    float height = 0;

    bool IsEmpty() const
    {
        return width <= 0 || height <= 0;
    }
};

}
