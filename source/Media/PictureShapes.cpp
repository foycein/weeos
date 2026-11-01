#include "Media/PictureShapes.h"

#include <algorithm>

namespace Media
{

namespace
{

struct ColorSums
{
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;
    int count = 0;

    void Add(const Formats::Rgba8Pixel& pixel)
    {
        red += pixel.red;
        green += pixel.green;
        blue += pixel.blue;
        alpha += pixel.alpha;
        ++count;
    }

    Formats::Byte Average(int sum) const
    {
        return static_cast<Formats::Byte>((sum + count / 2) / count);
    }

    Formats::Rgba8Pixel AveragePixel() const
    {
        return {Average(red), Average(green), Average(blue), Average(alpha)};
    }
};

const Formats::Rgba8Pixel& PixelAt(const Formats::RgbaImage& picture, std::uint32_t x, std::uint32_t y)
{
    return picture.pixels[static_cast<std::size_t>(y) * picture.width + x];
}

Formats::RgbaImage EmptyPicture(std::uint32_t width, std::uint32_t height)
{
    Formats::RgbaImage picture;
    picture.width = width;
    picture.height = height;
    picture.pixels.reserve(static_cast<std::size_t>(width) * height);
    return picture;
}

}

Formats::RgbaImage ShrinkPicture(const Formats::RgbaImage& picture, int factor)
{
    std::uint32_t wholeFactor = static_cast<std::uint32_t>(std::max(factor, 1));
    std::uint32_t width = (picture.width + wholeFactor - 1) / wholeFactor;
    std::uint32_t height = (picture.height + wholeFactor - 1) / wholeFactor;
    Formats::RgbaImage smaller = EmptyPicture(width, height);

    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            ColorSums sums;
            std::uint32_t lastY = std::min((y + 1) * wholeFactor, picture.height);
            std::uint32_t lastX = std::min((x + 1) * wholeFactor, picture.width);
            for (std::uint32_t sourceY = y * wholeFactor; sourceY < lastY; ++sourceY)
            {
                for (std::uint32_t sourceX = x * wholeFactor; sourceX < lastX; ++sourceX)
                    sums.Add(PixelAt(picture, sourceX, sourceY));
            }
            smaller.pixels.push_back(sums.AveragePixel());
        }
    }
    return smaller;
}

// The top row of the turned picture is the left column of the original, read from the bottom up.
Formats::RgbaImage TurnPictureRight(const Formats::RgbaImage& picture)
{
    Formats::RgbaImage turned = EmptyPicture(picture.height, picture.width);
    for (std::uint32_t y = 0; y < turned.height; ++y)
    {
        for (std::uint32_t x = 0; x < turned.width; ++x)
            turned.pixels.push_back(PixelAt(picture, y, picture.height - 1 - x));
    }
    return turned;
}

// The top row of the turned picture is the right column of the original, read from the top down.
Formats::RgbaImage TurnPictureLeft(const Formats::RgbaImage& picture)
{
    Formats::RgbaImage turned = EmptyPicture(picture.height, picture.width);
    for (std::uint32_t y = 0; y < turned.height; ++y)
    {
        for (std::uint32_t x = 0; x < turned.width; ++x)
            turned.pixels.push_back(PixelAt(picture, picture.width - 1 - y, x));
    }
    return turned;
}

Formats::RgbaImage TurnPictureUpsideDown(const Formats::RgbaImage& picture)
{
    Formats::RgbaImage turned = picture;
    std::reverse(turned.pixels.begin(), turned.pixels.end());
    return turned;
}

}
