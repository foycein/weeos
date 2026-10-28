#include "Layout/WindowDrawing.h"

#include "Layout/LayoutError.h"
#include "Layout/MaterialBuilder.h"
#include "Layout/PaneRectangle.h"
#include "Layout/RectangleDrawing.h"

#include <array>
#include <format>
#include <optional>
#include <vector>

namespace Layout
{

namespace
{

constexpr std::size_t NoFrames = 0;
constexpr std::size_t OneFrame = 1;
constexpr std::size_t FourFrames = 4;
constexpr std::size_t EightFrames = 8;

// The corners of a rectangle are counted like in the layout file: left top, right top, left bottom, right bottom.
// In the number of a corner, bit 0 tells the right side and bit 1 tells the bottom side.
constexpr std::size_t LeftTopCorner = 0;
constexpr std::size_t RightTopCorner = 1;
constexpr std::size_t LeftBottomCorner = 2;
constexpr std::size_t RightBottomCorner = 3;
constexpr std::size_t RightSideBit = 1;
constexpr std::size_t BottomSideBit = 2;

// The frames of a window with four frames are the four corners. A window with eight frames has the edges after the corners.
constexpr std::size_t LeftTopFrame = 0;
constexpr std::size_t RightTopFrame = 1;
constexpr std::size_t LeftBottomFrame = 2;
constexpr std::size_t RightBottomFrame = 3;
constexpr std::size_t LeftEdgeFrame = 4;
constexpr std::size_t RightEdgeFrame = 5;
constexpr std::size_t TopEdgeFrame = 6;
constexpr std::size_t BottomEdgeFrame = 7;

// A window with one frame turns its one texture for each of the four sides: not at all for the top, flipped horizontally
// for the right side, flipped vertically for the left side, and turned by 180 degrees for the bottom.
constexpr std::array<std::uint8_t, FourFrames> TurnsOfTheOneFrame = {0, 1, 2, 4};

using CornerCoordinates = std::array<Formats::Vector2, Formats::CornerCount>;

// The texture coordinates of the corners of a frame texture, for each of the ways that the layout file turns a texture:
// not turned, flipped horizontally, flipped vertically, turned by 90, 180 and 270 degrees.
constexpr std::array<CornerCoordinates, 6> CoordinatesOfTurnedTextures = {{
    {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}},
    {{{1, 0}, {0, 0}, {1, 1}, {0, 1}}},
    {{{0, 1}, {1, 1}, {0, 0}, {1, 0}}},
    {{{0, 1}, {0, 0}, {1, 1}, {1, 0}}},
    {{{1, 1}, {0, 1}, {1, 0}, {0, 0}}},
    {{{1, 0}, {1, 1}, {0, 0}, {0, 1}}},
}};

constexpr std::array<Formats::Rgba8Color, Formats::CornerCount> WhiteVertexColors = {{
    {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
}};

struct TextureSize
{
    float width = 0;
    float height = 0;
};

// How thick the frame is on each side of the window.
struct FrameThickness
{
    float left = 0;
    float right = 0;
    float top = 0;
    float bottom = 0;
};

// One rectangle of the frame. It is measured from the left top corner of the window rectangle, with y growing downward.
struct FramePiece
{
    std::size_t frameIndex;

    // The corner of the piece where the texture starts. The texture repeats from there along the sides of the piece.
    std::size_t startCorner;

    float distanceFromLeft;
    float distanceFromTop;
    float width;
    float height;
};

std::optional<TextureSize> FirstTextureSizeOf(LiveLayout& layout, std::uint16_t materialIndex)
{
    const LiveMaterial& material = layout.MaterialAt(materialIndex);
    if (material.description.textures.empty())
        return std::nullopt;

    LoadedTexture texture = ObtainTextureOfMap(layout, material, 0);
    return TextureSize{static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

void RequireSupportedFrameCount(const Formats::Window& window, const Formats::Pane& pane)
{
    std::size_t frameCount = window.frames.size();
    bool isSupported = frameCount == NoFrames || frameCount == OneFrame || frameCount == FourFrames || frameCount == EightFrames;
    if (!isSupported)
        throw LayoutError(std::format("The window {} has {} frames, but only 0, 1, 4 and 8 are supported", pane.name, frameCount));
}

// A window with one frame is drawn like a window with four frames that use the one texture.
std::vector<Formats::WindowFrame> FourFramesInsteadOfOne(const std::vector<Formats::WindowFrame>& frames)
{
    if (frames.size() != OneFrame)
        return frames;

    std::vector<Formats::WindowFrame> fourFrames;
    for (std::uint8_t turn : TurnsOfTheOneFrame)
        fourFrames.push_back({frames.front().materialIndex, turn});

    return fourFrames;
}

// The thickness on the left and at the top is the size of the texture of the left top frame,
// and the thickness on the right and at the bottom is the size of the texture of the right bottom frame.
FrameThickness MeasureFrameThickness(LiveLayout& layout, const std::vector<Formats::WindowFrame>& frames)
{
    FrameThickness thickness;
    if (frames.size() == NoFrames)
        return thickness;

    TextureSize leftTop = FirstTextureSizeOf(layout, frames[LeftTopFrame].materialIndex).value_or(TextureSize());
    TextureSize rightBottom = FirstTextureSizeOf(layout, frames[RightBottomFrame].materialIndex).value_or(TextureSize());
    thickness.left = leftTop.width;
    thickness.top = leftTop.height;
    thickness.right = rightBottom.width;
    thickness.bottom = rightBottom.height;
    return thickness;
}

// The corner at the other end of the same top or bottom side.
std::size_t CornerAcrossTheWidth(std::size_t corner)
{
    return corner ^ RightSideBit;
}

// The corner at the other end of the same left or right side.
std::size_t CornerAcrossTheHeight(std::size_t corner)
{
    return corner ^ BottomSideBit;
}

bool AreOnTheSameLeftOrRightSide(std::size_t firstCorner, std::size_t secondCorner)
{
    return (firstCorner & RightSideBit) == (secondCorner & RightSideBit);
}

bool AreOnTheSameTopOrBottomSide(std::size_t firstCorner, std::size_t secondCorner)
{
    return (firstCorner & BottomSideBit) == (secondCorner & BottomSideBit);
}

// Four bands, going around clockwise from the top: a band is as long as the window without the thickness of the frame
// at the end where the next band starts, so the bands do not overlap.
std::vector<FramePiece> MakeBandPieces(const FrameThickness& thickness, float windowWidth, float windowHeight)
{
    FramePiece top = {.frameIndex = LeftTopFrame, .startCorner = LeftTopCorner,
        .distanceFromLeft = 0, .distanceFromTop = 0,
        .width = windowWidth - thickness.right, .height = thickness.top};

    FramePiece right = {.frameIndex = RightTopFrame, .startCorner = RightTopCorner,
        .distanceFromLeft = windowWidth - thickness.right, .distanceFromTop = 0,
        .width = thickness.right, .height = windowHeight - thickness.bottom};

    FramePiece bottom = {.frameIndex = RightBottomFrame, .startCorner = RightBottomCorner,
        .distanceFromLeft = thickness.left, .distanceFromTop = windowHeight - thickness.bottom,
        .width = windowWidth - thickness.left, .height = thickness.bottom};

    FramePiece left = {.frameIndex = LeftBottomFrame, .startCorner = LeftBottomCorner,
        .distanceFromLeft = 0, .distanceFromTop = thickness.top,
        .width = thickness.left, .height = windowHeight - thickness.top};

    return {top, right, bottom, left};
}

// Four corners and four edges between them, going around clockwise from the left top corner.
std::vector<FramePiece> MakeCornerAndEdgePieces(const FrameThickness& thickness, float windowWidth, float windowHeight)
{
    float widthBetweenCorners = windowWidth - thickness.left - thickness.right;
    float heightBetweenCorners = windowHeight - thickness.top - thickness.bottom;
    float rightEdgeStart = windowWidth - thickness.right;
    float bottomEdgeStart = windowHeight - thickness.bottom;

    FramePiece leftTopCorner = {.frameIndex = LeftTopFrame, .startCorner = LeftTopCorner,
        .distanceFromLeft = 0, .distanceFromTop = 0,
        .width = thickness.left, .height = thickness.top};

    FramePiece topEdge = {.frameIndex = TopEdgeFrame, .startCorner = LeftTopCorner,
        .distanceFromLeft = thickness.left, .distanceFromTop = 0,
        .width = widthBetweenCorners, .height = thickness.top};

    FramePiece rightTopCorner = {.frameIndex = RightTopFrame, .startCorner = RightTopCorner,
        .distanceFromLeft = rightEdgeStart, .distanceFromTop = 0,
        .width = thickness.right, .height = thickness.top};

    FramePiece rightEdge = {.frameIndex = RightEdgeFrame, .startCorner = RightTopCorner,
        .distanceFromLeft = rightEdgeStart, .distanceFromTop = thickness.top,
        .width = thickness.right, .height = heightBetweenCorners};

    FramePiece rightBottomCorner = {.frameIndex = RightBottomFrame, .startCorner = RightBottomCorner,
        .distanceFromLeft = rightEdgeStart, .distanceFromTop = bottomEdgeStart,
        .width = thickness.right, .height = thickness.bottom};

    FramePiece bottomEdge = {.frameIndex = BottomEdgeFrame, .startCorner = RightBottomCorner,
        .distanceFromLeft = thickness.left, .distanceFromTop = bottomEdgeStart,
        .width = widthBetweenCorners, .height = thickness.bottom};

    FramePiece leftBottomCorner = {.frameIndex = LeftBottomFrame, .startCorner = LeftBottomCorner,
        .distanceFromLeft = 0, .distanceFromTop = bottomEdgeStart,
        .width = thickness.left, .height = thickness.bottom};

    FramePiece leftEdge = {.frameIndex = LeftEdgeFrame, .startCorner = LeftBottomCorner,
        .distanceFromLeft = 0, .distanceFromTop = thickness.top,
        .width = thickness.left, .height = heightBetweenCorners};

    return {leftTopCorner, topEdge, rightTopCorner, rightEdge, rightBottomCorner, bottomEdge, leftBottomCorner, leftEdge};
}

// The texture coordinates change by one whole texture when they move across one texture. This is how much they change
// when they move from one corner to another corner of the turned texture, over a side of the given length:
// one of the two numbers is positive or negative, depending on which way the texture runs, and the other is zero.
Formats::Vector2 CoordinateChangeAlongSide(Formats::Vector2 from, Formats::Vector2 to, float sideLength, const TextureSize& textureSize)
{
    Formats::Vector2 direction = {to.x - from.x, to.y - from.y};
    float textureLength = direction.x != 0 ? textureSize.width : textureSize.height;
    float textureCount = sideLength / textureLength;
    return {direction.x * textureCount, direction.y * textureCount};
}

// The start corner of the piece gets the coordinates of the same corner of the turned texture.
// From there the coordinates go along the sides of the texture, for as many textures as fit into the side of the piece.
Formats::TextureCoordinateSet MakePieceCoordinates(const CornerCoordinates& turnedTexture, const FramePiece& piece, const TextureSize& textureSize)
{
    Formats::Vector2 start = turnedTexture[piece.startCorner];
    Formats::Vector2 otherEndOfWidth = turnedTexture[CornerAcrossTheWidth(piece.startCorner)];
    Formats::Vector2 otherEndOfHeight = turnedTexture[CornerAcrossTheHeight(piece.startCorner)];
    Formats::Vector2 alongWidth = CoordinateChangeAlongSide(start, otherEndOfWidth, piece.width, textureSize);
    Formats::Vector2 alongHeight = CoordinateChangeAlongSide(start, otherEndOfHeight, piece.height, textureSize);

    Formats::TextureCoordinateSet set;
    for (std::size_t corner = 0; corner < Formats::CornerCount; ++corner)
    {
        Formats::Vector2 coordinates = start;
        if (!AreOnTheSameLeftOrRightSide(corner, piece.startCorner))
        {
            coordinates.x += alongWidth.x;
            coordinates.y += alongWidth.y;
        }
        if (!AreOnTheSameTopOrBottomSide(corner, piece.startCorner))
        {
            coordinates.x += alongHeight.x;
            coordinates.y += alongHeight.y;
        }
        set.corners[corner] = coordinates;
    }
    return set;
}

void DrawFramePiece(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement,
    const PaneRectangle& windowRectangle, const std::vector<Formats::WindowFrame>& frames, const FramePiece& piece)
{
    const Formats::WindowFrame& frame = frames[piece.frameIndex];
    std::optional<TextureSize> textureSize = FirstTextureSizeOf(layout, frame.materialIndex);
    if (!textureSize)
        return;

    if (frame.textureFlip >= CoordinatesOfTurnedTextures.size())
        throw LayoutError(std::format("The window {} turns a frame texture in the way {}, which is not known", placement.pane->name, frame.textureFlip));

    std::array<Formats::TextureCoordinateSet, 1> coordinateSets = {
        MakePieceCoordinates(CoordinatesOfTurnedTextures[frame.textureFlip], piece, *textureSize),
    };

    PaneRectangle pieceRectangle;
    pieceRectangle.left = windowRectangle.left + piece.distanceFromLeft;
    pieceRectangle.top = windowRectangle.top - piece.distanceFromTop;
    pieceRectangle.width = piece.width;
    pieceRectangle.height = piece.height;

    RectangleAppearance appearance;
    appearance.materialIndex = frame.materialIndex;
    appearance.vertexColors = WhiteVertexColors;
    appearance.textureCoordinateSets = coordinateSets;
    DrawRectangle(device, layout, placement, pieceRectangle, appearance);
}

void DrawFrames(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement,
    const PaneRectangle& windowRectangle, const std::vector<Formats::WindowFrame>& frames, const FrameThickness& thickness)
{
    if (frames.size() == NoFrames)
        return;

    std::vector<FramePiece> pieces = frames.size() == EightFrames
        ? MakeCornerAndEdgePieces(thickness, windowRectangle.width, windowRectangle.height)
        : MakeBandPieces(thickness, windowRectangle.width, windowRectangle.height);

    for (const FramePiece& piece : pieces)
        DrawFramePiece(device, layout, placement, windowRectangle, frames, piece);
}

// The content lies inside the frame. The magnification of the window makes it bigger, over the inner edge of the frame.
void DrawContent(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement,
    const PaneRectangle& windowRectangle, const FrameThickness& thickness)
{
    const Formats::Window& window = *placement.pane->window;

    PaneRectangle contentRectangle;
    contentRectangle.left = windowRectangle.left + thickness.left - window.magnificationLeft;
    contentRectangle.top = windowRectangle.top - thickness.top + window.magnificationUp;
    contentRectangle.width = windowRectangle.width - thickness.left - thickness.right + window.magnificationLeft + window.magnificationRight;
    contentRectangle.height = windowRectangle.height - thickness.top - thickness.bottom + window.magnificationUp + window.magnificationDown;

    RectangleAppearance appearance;
    appearance.materialIndex = window.content.materialIndex;
    appearance.vertexColors = window.content.vertexColors;
    appearance.textureCoordinateSets = window.content.textureCoordinateSets;
    DrawRectangle(device, layout, placement, contentRectangle, appearance);
}

}

void DrawWindow(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement)
{
    const Formats::Pane& pane = *placement.pane;
    RequireSupportedFrameCount(*pane.window, pane);

    PaneRectangle windowRectangle = RectangleOfPane(pane);
    std::vector<Formats::WindowFrame> frames = FourFramesInsteadOfOne(pane.window->frames);
    FrameThickness thickness = MeasureFrameThickness(layout, frames);

    DrawContent(device, layout, placement, windowRectangle, thickness);
    DrawFrames(device, layout, placement, windowRectangle, frames, thickness);
}

}
