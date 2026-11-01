#pragma once

#include "Graphics/Matrix4.h"
#include "Graphics/TevStage.h"

#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"
#include "Platform/ScreenShape.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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

    // The colors as StandardAlpha, but the alpha becomes the coverage of both: sourceAlpha + destinationAlpha * (1 - sourceAlpha).
    // An offscreen picture drawn this way can be laid over another picture later. The Mii icons are drawn this way.
    Over,

    // The source goes behind what is already there: color = source * (1 - destinationAlpha) + destination * destinationAlpha,
    // alpha = sourceAlpha * (1 - destinationAlpha) + destinationAlpha. What is drawn first stays in front.
    // The face texture of a Mii is drawn this way.
    Behind,
};

constexpr std::size_t BlendModeCount = 5;

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

    // The alpha test of the Wii's graphics chip as the Mii face library uses it: a pixel whose alpha is 0 is not drawn at all,
    // so it changes neither the colors nor the depth.
    bool skipsTransparentPixels = false;
};

// ---- Meshes (3D)

// One corner of a triangle of a mesh.
struct MeshVertex
{
    Formats::Vector3 position;

    // Points out of the front side. It is only read for the light and for texture coordinates from the normal.
    Formats::Vector3 normal;

    // Every texture of the material reads these, each through its own texture matrix.
    Formats::Vector2 textureCoordinate;
};

// The front side of a triangle is the side from which its corners go around clockwise in the picture (the rule of the
// Wii's graphics chip).
enum class FaceCulling : std::uint8_t
{
    None,
    Back,
    Front,
};

// Every picture has a depth buffer that starts at the far end. Quads do not use it.
enum class DepthMode : std::uint8_t
{
    // A pixel is drawn only when it is not behind what was drawn there before, and its depth is kept.
    TestAndWrite,

    // A pixel is drawn only when it is not behind what was drawn there before; the depth stays as it was.
    TestOnly,
};

// A white or colored light at a point, worked out at every corner the way the Wii does it:
//     color = ambientColor + lightColor * max(0, normal . direction to the light), limited to 1.
// The result is the raster color of the stages, so the material decides what it does with it.
struct PointLight
{
    // In view space.
    Formats::Vector3 position;
    Formats::Rgba8Color color;
    Formats::Rgba8Color ambientColor;
};

// Where a mesh lands in the picture and how it is lit.
struct MeshPlacement
{
    // From the units of the mesh into view space: the eye is at the origin, x points right, y up, and the eye looks along -z.
    Matrix4 modelView;

    // From view space into the picture: after dividing by the fourth number x and y run from -1 (left, bottom) to 1 (right, top)
    // over the whole canvas, and the depth from 0 (near) to 1 (far). MakePerspective makes one.
    Matrix4 projection;

    // Without a light the raster color is opaque white.
    std::optional<PointLight> light;

    // The texture coordinates are the x and y of the normal in view space instead of the ones of the corners, for pictures of
    // a lit sphere that follow the surface (environment maps). The texture matrices then bring them into the picture.
    bool textureCoordinatesFromNormal = false;

    FaceCulling culling = FaceCulling::Back;
    DepthMode depth = DepthMode::TestAndWrite;
};

// ---- The device

// Draws quads and meshes. Only one thread may use a device. Every function can throw a GraphicsError.
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
    virtual void BeginFrame(const Canvas& canvas, Platform::ScreenShape screenShape) = 0;

    // The scissor holds until ClearScissor or until the next picture or offscreen picture starts.
    // Every picture has a scissor of its own: an offscreen picture starts without one, and after EndOffscreen
    // the screen picture goes on with the scissor it had before.
    virtual void SetScissor(const ScissorRectangle& scissor) = 0;
    virtual void ClearScissor() = 0;

    virtual void DrawQuad(const Quad& quad, const Material& material) = 0;

    // Draws triangles, three corners each, with the depth buffer of the picture. The device copies the corners during the call.
    virtual void DrawMesh(std::span<const MeshVertex> triangleCorners, const MeshPlacement& placement, const Material& material) = 0;

    // Draws into a new picture of width * height pixels instead of the screen, until EndOffscreen.
    // The canvas covers the whole picture. The picture starts transparent black.
    // The returned texture holds the picture after EndOffscreen. It is an ordinary texture then:
    // use it in materials and destroy it with DestroyTexture. It must not be used while it is being drawn.
    // This may also be called between BeginFrame and Present: the screen picture goes on after EndOffscreen.
    // Outside of BeginFrame and Present the picture is finished when EndOffscreen returns.
    // An offscreen picture can be drawn inside another one: the outer picture goes on after EndOffscreen.
    virtual TextureHandle BeginOffscreen(const Canvas& canvas, std::uint32_t width, std::uint32_t height) = 0;
    virtual void EndOffscreen() = 0;

    // Shows the picture. It waits for the next refresh of the display, so the pictures come at the speed of the display.
    virtual void Present() = 0;
};

}
