#pragma once

#include "Graphics/TevStage.h"

#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Graphics
{

constexpr std::size_t QuadCornerCount = 4;
constexpr std::size_t MaximumTextureCount = 3;
constexpr std::size_t MaximumStageCount = 16;
constexpr std::size_t ColorRegisterCount = 3;
constexpr std::size_t ConstantColorCount = 4;

// ---- Textures

// Names one picture that lives on the graphics device. A handle with the number zero names nothing.
struct TextureHandle
{
    std::uint32_t number = 0;

    bool IsValid() const
    {
        return number != 0;
    }

    bool operator==(const TextureHandle& other) const = default;
};

enum class WrapMode : std::uint8_t
{
    // The pixels at the edge are repeated.
    Clamp,

    // The picture is repeated.
    Repeat,

    // The picture is repeated and every second copy is flipped.
    Mirror,
};

enum class FilterMode : std::uint8_t
{
    Nearest,

    // Bilinear: the four nearest pixels are blended. There are no smaller copies of the picture (no mipmaps).
    Linear,
};

struct TextureDescription
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // width * height pixels, row by row from the top left corner. The device copies them during CreateTexture.
    std::span<const Formats::Rgba8Pixel> pixels;

    WrapMode horizontalWrap = WrapMode::Clamp;
    WrapMode verticalWrap = WrapMode::Clamp;
    FilterMode filter = FilterMode::Linear;
};

// ---- The picture area

// The coordinates the quads are given in, the layout units. The origin is the middle of the picture, x grows to the right
// and y grows upward: x runs from -width / 2 to width / 2 and y from -height / 2 (bottom) to height / 2 (top).
struct Canvas
{
    float width = 0;
    float height = 0;
};

// The shape the picture has on the screen. The picture is stretched to it the way the console's video output does.
// A window of another shape gets black bars.
enum class ScreenShape : std::uint8_t
{
    FourByThree,
    SixteenByNine,
};

// Limits the drawing to a rectangle, in canvas units.
struct ScissorRectangle
{
    float left = 0;
    float right = 0;
    float bottom = 0;
    float top = 0;
};

// ---- What to draw

// A quad has four corners. They are given in this order: left top, right top, right bottom, left bottom,
// as the pane is seen before it is rotated. Texture coordinates have their origin at the top left corner of the picture:
// (0, 0) is the top left and (1, 1) the bottom right.
struct Quad
{
    // In canvas units, after the pane's matrix has been applied.
    std::array<Formats::Vector2, QuadCornerCount> positions;

    // The raster colors. They are blended across the quad from corner to corner.
    std::array<Formats::Rgba8Color, QuadCornerCount> colors;

    // Texture number n of the material is read with set n. A set the material does not read is ignored.
    std::array<std::array<Formats::Vector2, QuadCornerCount>, MaximumTextureCount> textureCoordinates;
};

// Moves, scales and rotates texture coordinates. The first row makes the new s and the second row the new t:
//     newS = rows[0][0] * s + rows[0][1] * t + rows[0][2]
//     newT = rows[1][0] * s + rows[1][1] * t + rows[1][2]
struct TextureMatrix
{
    std::array<std::array<float, 3>, 2> rows = {{{1, 0, 0}, {0, 1, 0}}};
};

struct MaterialTexture
{
    TextureHandle texture;
    TextureMatrix matrix;
};

// The color registers hold signed numbers, because the stages can calculate beyond black and white.
// 255 is full intensity. The range is -1024 to 1023.
struct RegisterColor
{
    std::int16_t red = 0;
    std::int16_t green = 0;
    std::int16_t blue = 0;
    std::int16_t alpha = 0;
};

enum class BlendMode : std::uint8_t
{
    // result = source * sourceAlpha + destination * (1 - sourceAlpha), on the colors as they are (no gamma conversion).
    StandardAlpha,

    // The source replaces what was there.
    Replace,

    // result = source * sourceAlpha + destination, on the colors as they are. The source is added to what was there, as much as its
    // alpha says. The banners of some channels use it to make a logo flash.
    AddSourceAlpha,
};

// A material says how a quad gets its colors.
struct Material
{
    // Up to MaximumTextureCount textures. Texture n is read with the texture coordinates n of the quad.
    std::vector<MaterialTexture> textures;

    // The registers REG0, REG1 and REG2 of the stages: black, white and white as long as nobody changes them.
    std::array<RegisterColor, ColorRegisterCount> colorRegisters = {{{0, 0, 0, 0}, {255, 255, 255, 255}, {255, 255, 255, 255}}};

    std::array<Formats::Rgba8Color, ConstantColorCount> constantColors = {{
        {255, 255, 255, 255},
        {255, 255, 255, 255},
        {255, 255, 255, 255},
        {255, 255, 255, 255},
    }};

    // Up to MaximumStageCount stages, run in order. When there are none, the device uses the standard chain
    // for the number of textures: nothing but the raster color for no texture, the texture tinted by REG0 and REG1 for one,
    // and so on. The layout library uses the same default.
    std::vector<TevStage> stages;

    BlendMode blendMode = BlendMode::StandardAlpha;
};

// ---- The device

// Draws quads. Only one thread may use a device. Every function can throw a GraphicsError.
class GraphicsDevice
{
public:
    GraphicsDevice() = default;
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;
    virtual ~GraphicsDevice() = default;

    virtual TextureHandle CreateTexture(const TextureDescription& description) = 0;

    // The device keeps the texture alive until the pictures that use it are finished, so a texture may be destroyed
    // right after the last quad that used it.
    virtual void DestroyTexture(TextureHandle texture) = 0;

    // Starts a new picture of the screen and clears it to black. Everything drawn until Present is part of it.
    virtual void BeginFrame(const Canvas& canvas, ScreenShape screenShape) = 0;

    // The scissor holds until ClearScissor or until the next picture or offscreen picture starts.
    virtual void SetScissor(const ScissorRectangle& scissor) = 0;
    virtual void ClearScissor() = 0;

    virtual void DrawQuad(const Quad& quad, const Material& material) = 0;

    // Draws into a new picture of width * height pixels instead of the screen, until EndOffscreen.
    // The canvas covers the whole picture. The picture starts transparent black.
    // The returned texture holds the picture after EndOffscreen. It is an ordinary texture then:
    // use it in materials and destroy it with DestroyTexture. It must not be used while it is being drawn.
    // This may also be called between BeginFrame and Present: the screen picture goes on after EndOffscreen.
    virtual TextureHandle BeginOffscreen(const Canvas& canvas, std::uint32_t width, std::uint32_t height) = 0;
    virtual void EndOffscreen() = 0;

    // Shows the picture. It waits for the next refresh of the display, so the pictures come at the speed of the display.
    virtual void Present() = 0;
};

}
