#pragma once

#include "SharpArt/VectorCanvas.h"

#include "Formats/RgbaImage.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace SharpArt
{

// How the layouts read the bytes of a picture.
enum class PictureChannels
{
    // Red, green, blue and alpha, as painted.
    ColorAndAlpha,

    // One gray level in all four bytes, like the I4 and I8 textures of the Wii: the drawing is painted in white and grays, and
    // every pixel becomes its gray times its alpha.
    Intensity,
};

// A picture of the Wii that WeeOS draws itself, sharp at any size: the same shapes and colors as the original, which is small
// because it was made for a television.
struct SharpPicture
{
    // The name of the texture file it replaces, for example "my_Clock_a0.tpl".
    std::string_view textureFileName;

    // The size of the original in pixels. The drawing uses the same units, so it lands exactly where the original does.
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // FingerprintOf the original, so a picture of another title that has the same name and size is never replaced.
    std::uint32_t originalFingerprint = 0;

    // How many layout units the menu shows one pixel of the original at, at the most. Most pictures are shown at their size (1);
    // the digits of the clock are stretched to almost twice their size.
    float largestMagnification = 1;

    PictureChannels channels = PictureChannels::ColorAndAlpha;

    void (*draw)(VectorCanvas& canvas) = nullptr;

    // The color of the pixels where nothing is painted. A white logo has white there, as its original does, so its edges do not turn
    // dark where the graphics card blends a pixel with its transparent neighbours.
    Color transparentColor = {0, 0, 0, 0};
};

// Every sharp picture that WeeOS has.
std::span<const SharpPicture> AllSharpPictures();

// The sharp picture for the texture file, or nothing when WeeOS draws none for it.
const SharpPicture* FindSharpPicture(std::string_view textureFileName);

// The CRC-32 of the red, green, blue and alpha bytes of every pixel, row by row.
std::uint32_t FingerprintOf(const Formats::RgbaImage& picture);

// True when the picture is the original that the sharp picture replaces: the same size and the same fingerprint.
bool IsTheOriginalOf(const SharpPicture& sharpPicture, const Formats::RgbaImage& picture);

// True for a picture of words: its file name starts with "tx_" (the words of the HOME menu) and it is an intensity picture. Words
// cannot be drawn again without copying the font, so such a picture gets the sharp edges of the fonts instead (docs/audit/SharpArt.md).
bool IsPictureOfWords(std::string_view textureFileName, const Formats::RgbaImage& picture);

// Draws the picture with pixelsPerUnit pixels for every pixel of the original.
Formats::RgbaImage DrawSharpPicture(const SharpPicture& sharpPicture, std::uint32_t pixelsPerUnit);

// How many pixels per pixel of the original the picture needs on a screen that shows a layout unit with screenPixelsPerUnit pixels,
// so it is never stretched by the graphics card. At least 1, at most MostPixelsPerUnit.
std::uint32_t PixelsPerUnitFor(const SharpPicture& sharpPicture, float screenPixelsPerUnit);

constexpr std::uint32_t MostPixelsPerUnit = 16;

}
