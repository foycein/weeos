#pragma once

#include "Graphics/CanvasMapping.h"
#include "Graphics/GraphicsDevice.h"

#include <cstdint>

namespace Graphics
{

// How the console makes its picture. The numbers come from the System Menu 4.3U:
// the menu draws into a frame buffer of 640 x 456 pixels and copies it with a vertical filter. The video chip shows the copy
// as a part of a picture of 720 x 480 pixels, with black around it, and the television stretches that picture over its whole screen.

constexpr std::uint32_t ConsoleFrameWidth = 640;
constexpr std::uint32_t ConsoleFrameHeight = 456;

// The picture of the video chip.
constexpr Canvas ConsoleRasterCanvas = {720, 480};

// Assumed: how the pixels in between are made when the frame buffer is stretched to the screen.
// The video chip scales with a filter of its own, and bilinear filtering gives the same smooth edges.
// What the copy reads beyond the edges of the frame buffer is DrawTargetEdgeWrap in TextureStore.cpp.
constexpr FilterMode ConsoleScalingFilter = FilterMode::Linear;

// The units of the 16:9 picture of the menu. The menu draws one unit as one pixel in the height of its frame buffer, and it
// stretches the width of the canvas over the 640 pixels.
constexpr Canvas ConsoleCanvas = {832, 456};

// Which way the console sends its picture to the television. The vertical filter of the copy depends on it.
// The console of the dump has no setting for the progressive mode, so it sends 480i.
enum class ConsoleScan : std::uint8_t
{
    Progressive,
    Interlaced,
};

constexpr ConsoleScan ReferenceConsoleScan = ConsoleScan::Interlaced;

// How the device makes the picture of the screen.
enum class PictureResolution : std::uint8_t
{
    // The picture is drawn with the pixels of the window, so the edges are sharp. It fills the whole 16:9 area of the window.
    Window,

    // The picture is drawn into a frame buffer of 640 x 456 pixels, filtered vertically as the console copies it, and shown
    // in the part of the screen that the video chip uses, with the black border around it. Use it to compare with the console.
    Console,
};

struct PictureSettings
{
    PictureResolution resolution = PictureResolution::Window;
    ConsoleScan consoleScan = ReferenceConsoleScan;
};

// The pixels of the window that show the frame buffer of the console, when the 16:9 picture fills screenArea.
// The pointer has to be converted with this area while the picture is made in the console way.
Platform::PixelRectangle ConsolePictureArea(const Platform::PixelRectangle& screenArea);

// The corners of the frame buffer in the units of ConsoleRasterCanvas, with the texture coordinates of the whole frame buffer.
Quad MakeConsolePictureQuad();

// How much of the line above, of the line itself and of the line below make a line of the copy. The numbers are out of 255
// and add up to exactly 255, as the constants of a stage hold them.
struct ConsoleFilterWeights
{
    std::uint8_t lineAbove = 0;
    std::uint8_t lineItself = 0;
    std::uint8_t lineBelow = 0;
};

ConsoleFilterWeights FindConsoleFilterWeights(ConsoleScan consoleScan);

// The material that makes the copy of the console from the frame buffer. Draw it with MakeConsolePictureQuad.
// It replaces what was there, so nothing has to be cleared before.
Material MakeConsoleFilterMaterial(TextureHandle frameBuffer, ConsoleScan consoleScan);

}
