#include "Mii/MiiTextureUpload.h"

namespace Mii
{

namespace
{

Formats::Byte ChannelOf(const Formats::Rgba8Pixel& pixel, ColorChannel channel)
{
    switch (channel)
    {
    case ColorChannel::Red:
        return pixel.red;
    case ColorChannel::Green:
        return pixel.green;
    case ColorChannel::Blue:
        return pixel.blue;
    }
    return 0;
}

Graphics::WrapMode ToWrapMode(Formats::TextureWrap wrap)
{
    switch (wrap)
    {
    case Formats::TextureWrap::Clamp:
        return Graphics::WrapMode::Clamp;
    case Formats::TextureWrap::Repeat:
        return Graphics::WrapMode::Repeat;
    case Formats::TextureWrap::Mirror:
        return Graphics::WrapMode::Mirror;
    }
    return Graphics::WrapMode::Clamp;
}

Graphics::TextureHandle Upload(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image, Graphics::WrapMode horizontalWrap,
    Graphics::WrapMode verticalWrap)
{
    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;
    description.horizontalWrap = horizontalWrap;
    description.verticalWrap = verticalWrap;
    description.filter = Graphics::FilterMode::Linear;
    return device.CreateTexture(description);
}

}

Formats::RgbaImage SpreadChannel(const Formats::RgbaImage& image, ColorChannel channel)
{
    Formats::RgbaImage spread = image;
    for (Formats::Rgba8Pixel& pixel : spread.pixels)
    {
        Formats::Byte value = ChannelOf(pixel, channel);
        pixel = {value, value, value, pixel.alpha};
    }
    return spread;
}

Formats::RgbaImage SwapGreenAndAlpha(const Formats::RgbaImage& image)
{
    Formats::RgbaImage swapped = image;
    for (Formats::Rgba8Pixel& pixel : swapped.pixels)
        pixel = {pixel.red, pixel.alpha, pixel.blue, pixel.green};
    return swapped;
}

Graphics::TextureHandle UploadClampedTexture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image)
{
    return Upload(device, image, Graphics::WrapMode::Clamp, Graphics::WrapMode::Clamp);
}

Graphics::TextureHandle UploadTexture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image, const MiiTexture& storedSettings)
{
    return Upload(device, image, ToWrapMode(storedSettings.horizontalWrap), ToWrapMode(storedSettings.verticalWrap));
}

}
