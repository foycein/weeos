#include "SystemMenu/EdgeExtension.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace SystemMenu
{

namespace
{

// Panes are placed in whole layout units, often a little beyond the edge; a pane that ends this close to an edge of the canvas reaches it.
constexpr float EdgeTolerance = 0.5f;

// The corners of a quad that is not turned have the same height on top and at the bottom, and the same x on each side.
constexpr float CornerTolerance = 0.01f;

// The corners of a quad in the order of Graphics::Quad::positions.
constexpr std::size_t LeftTop = 0;
constexpr std::size_t RightTop = 1;
constexpr std::size_t RightBottom = 2;
constexpr std::size_t LeftBottom = 3;

// The sides of a quad that is not turned, in canvas units (y grows upward).
struct Box
{
    float left = 0;
    float right = 0;
    float bottom = 0;
    float top = 0;
};

// One stretch of x or y, for one row or column of the parts.
struct Stretch
{
    float from = 0;
    float to = 0;
};

bool AreClose(float first, float second)
{
    return std::abs(first - second) <= CornerTolerance;
}

std::optional<Box> BoxOfUprightQuad(const Graphics::Quad& quad)
{
    const std::array<Formats::Vector2, Graphics::QuadCornerCount>& corners = quad.positions;
    bool topIsLevel = AreClose(corners[LeftTop].y, corners[RightTop].y);
    bool bottomIsLevel = AreClose(corners[LeftBottom].y, corners[RightBottom].y);
    bool leftIsUpright = AreClose(corners[LeftTop].x, corners[LeftBottom].x);
    bool rightIsUpright = AreClose(corners[RightTop].x, corners[RightBottom].x);
    if (!topIsLevel || !bottomIsLevel || !leftIsUpright || !rightIsUpright)
        return std::nullopt;

    Box box = {corners[LeftTop].x, corners[RightTop].x, corners[LeftBottom].y, corners[LeftTop].y};
    bool hasArea = box.left < box.right && box.bottom < box.top;
    if (!hasArea)
        return std::nullopt;

    return box;
}

// A place of the plane of the quad as shares of its width (0 on the left side, 1 on the right side) and of its height (0 on top,
// 1 at the bottom). Beyond the quad the shares are below 0 or above 1.
struct Shares
{
    float across = 0;
    float down = 0;
};

Shares SharesOf(const Box& box, Formats::Vector2 place)
{
    Shares shares;
    shares.across = (place.x - box.left) / (box.right - box.left);
    shares.down = (box.top - place.y) / (box.top - box.bottom);
    return shares;
}

Formats::Vector2 Blend(Formats::Vector2 first, Formats::Vector2 second, float share)
{
    return {first.x + (second.x - first.x) * share, first.y + (second.y - first.y) * share};
}

// Texture coordinates go on in a straight line beyond the quad.
Formats::Vector2 TextureCoordinateAt(const std::array<Formats::Vector2, Graphics::QuadCornerCount>& corners, Shares shares)
{
    Formats::Vector2 onTop = Blend(corners[LeftTop], corners[RightTop], shares.across);
    Formats::Vector2 atBottom = Blend(corners[LeftBottom], corners[RightBottom], shares.across);
    return Blend(onTop, atBottom, shares.down);
}

std::uint8_t BlendChannel(std::uint8_t first, std::uint8_t second, float share)
{
    float blended = first + (second - first) * share;
    return static_cast<std::uint8_t>(std::lround(blended));
}

Formats::Rgba8Color BlendColor(const Formats::Rgba8Color& first, const Formats::Rgba8Color& second, float share)
{
    Formats::Rgba8Color blended;
    blended.red = BlendChannel(first.red, second.red, share);
    blended.green = BlendChannel(first.green, second.green, share);
    blended.blue = BlendChannel(first.blue, second.blue, share);
    blended.alpha = BlendChannel(first.alpha, second.alpha, share);
    return blended;
}

// The colors stay those of the nearest edge of the quad.
Formats::Rgba8Color ColorAt(const std::array<Formats::Rgba8Color, Graphics::QuadCornerCount>& corners, Shares shares)
{
    float across = std::clamp(shares.across, 0.0f, 1.0f);
    float down = std::clamp(shares.down, 0.0f, 1.0f);
    Formats::Rgba8Color onTop = BlendColor(corners[LeftTop], corners[RightTop], across);
    Formats::Rgba8Color atBottom = BlendColor(corners[LeftBottom], corners[RightBottom], across);
    return BlendColor(onTop, atBottom, down);
}

Graphics::Quad MakePart(const Graphics::Quad& quad, const Box& box, Stretch columns, Stretch rows)
{
    std::array<Formats::Vector2, Graphics::QuadCornerCount> places = {{{columns.from, rows.from}, {columns.to, rows.from}, {columns.to, rows.to},
        {columns.from, rows.to}}};

    Graphics::Quad part = {};
    for (std::size_t corner = 0; corner < Graphics::QuadCornerCount; ++corner)
    {
        Shares shares = SharesOf(box, places[corner]);
        part.positions[corner] = places[corner];
        part.colors[corner] = ColorAt(quad.colors, shares);
        for (std::size_t textureNumber = 0; textureNumber < Graphics::MaximumTextureCount; ++textureNumber)
            part.textureCoordinates[textureNumber][corner] = TextureCoordinateAt(quad.textureCoordinates[textureNumber], shares);
    }
    return part;
}

Graphics::ScissorRectangle AreaOfCanvas(const Graphics::Canvas& canvas)
{
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;
    return {-halfWidth, halfWidth, -halfHeight, halfHeight};
}

}

std::vector<Graphics::Quad> MakeEdgeExtensions(const Graphics::Quad& quad, const Graphics::ScissorRectangle& canvasArea,
    const Graphics::ScissorRectangle& wholeArea)
{
    std::optional<Box> found = BoxOfUprightQuad(quad);
    if (!found)
        return {};

    const Box& box = *found;
    bool reachesLeft = box.left <= canvasArea.left + EdgeTolerance;
    bool reachesRight = box.right >= canvasArea.right - EdgeTolerance;
    bool reachesTop = box.top >= canvasArea.top - EdgeTolerance;
    bool reachesBottom = box.bottom <= canvasArea.bottom + EdgeTolerance;

    bool isAcrossTheWidth = reachesLeft && reachesRight;
    bool isAcrossTheHeight = reachesTop && reachesBottom;
    if (!isAcrossTheWidth && !isAcrossTheHeight)
        return {};

    bool goesLeft = reachesLeft && wholeArea.left < box.left - EdgeTolerance;
    bool goesRight = reachesRight && wholeArea.right > box.right + EdgeTolerance;
    bool goesUp = reachesTop && wholeArea.top > box.top + EdgeTolerance;
    bool goesDown = reachesBottom && wholeArea.bottom < box.bottom - EdgeTolerance;

    std::vector<Stretch> columns;
    if (goesLeft)
        columns.push_back({wholeArea.left, box.left});
    columns.push_back({box.left, box.right});
    if (goesRight)
        columns.push_back({box.right, wholeArea.right});

    std::vector<Stretch> rows;
    if (goesUp)
        rows.push_back({wholeArea.top, box.top});
    rows.push_back({box.top, box.bottom});
    if (goesDown)
        rows.push_back({box.bottom, wholeArea.bottom});

    std::vector<Graphics::Quad> parts;
    for (const Stretch& row : rows)
    {
        for (const Stretch& column : columns)
        {
            bool isTheQuadItself = column.from == box.left && column.to == box.right && row.from == box.top && row.to == box.bottom;
            if (!isTheQuadItself)
                parts.push_back(MakePart(quad, box, column, row));
        }
    }
    return parts;
}

Graphics::ScissorRectangle ExtendScissor(const Graphics::ScissorRectangle& scissor, const Graphics::ScissorRectangle& canvasArea,
    const Graphics::ScissorRectangle& wholeArea)
{
    Graphics::ScissorRectangle extended = scissor;
    if (scissor.left <= canvasArea.left + EdgeTolerance)
        extended.left = std::min(scissor.left, wholeArea.left);
    if (scissor.right >= canvasArea.right - EdgeTolerance)
        extended.right = std::max(scissor.right, wholeArea.right);
    if (scissor.bottom <= canvasArea.bottom + EdgeTolerance)
        extended.bottom = std::min(scissor.bottom, wholeArea.bottom);
    if (scissor.top >= canvasArea.top - EdgeTolerance)
        extended.top = std::max(scissor.top, wholeArea.top);
    return extended;
}

EdgeExtendingDevice::EdgeExtendingDevice(Graphics::GraphicsDevice& targetDevice, const Graphics::Canvas& startingCanvas)
    : target(targetDevice)
    , canvasArea(AreaOfCanvas(startingCanvas))
{
}

Graphics::TextureHandle EdgeExtendingDevice::CreateTexture(const Graphics::TextureDescription& description)
{
    return target.CreateTexture(description);
}

void EdgeExtendingDevice::DestroyTexture(Graphics::TextureHandle texture)
{
    target.DestroyTexture(texture);
}

void EdgeExtendingDevice::BeginFrame(const Graphics::Canvas& canvas)
{
    canvasArea = AreaOfCanvas(canvas);
    scissor.reset();
    target.BeginFrame(canvas);
}

float EdgeExtendingDevice::ScreenPixelsPerUnit() const
{
    return target.ScreenPixelsPerUnit();
}

Graphics::ScissorRectangle EdgeExtendingDevice::WholeArea() const
{
    return target.WholeArea();
}

void EdgeExtendingDevice::SetScissor(const Graphics::ScissorRectangle& newScissor)
{
    scissor = newScissor;
    target.SetScissor(newScissor);
}

void EdgeExtendingDevice::ClearScissor()
{
    scissor.reset();
    target.ClearScissor();
}

void EdgeExtendingDevice::DrawQuad(const Graphics::Quad& quad, const Graphics::Material& material)
{
    target.DrawQuad(quad, material);

    Graphics::ScissorRectangle wholeArea = target.WholeArea();
    std::vector<Graphics::Quad> extensions = MakeEdgeExtensions(quad, canvasArea, wholeArea);
    if (extensions.empty())
        return;

    Graphics::ScissorRectangle limit = scissor.value_or(canvasArea);
    target.SetScissor(ExtendScissor(limit, canvasArea, wholeArea));
    for (const Graphics::Quad& extension : extensions)
        target.DrawQuad(extension, material);

    if (scissor)
        target.SetScissor(*scissor);
    else
        target.ClearScissor();
}

void EdgeExtendingDevice::DrawMesh(std::span<const Graphics::MeshVertex> triangleCorners, const Graphics::MeshPlacement& placement,
    const Graphics::Material& material)
{
    target.DrawMesh(triangleCorners, placement, material);
}

Graphics::MeshHandle EdgeExtendingDevice::CreateMesh(std::span<const Graphics::MeshVertex> triangleCorners)
{
    return target.CreateMesh(triangleCorners);
}

void EdgeExtendingDevice::DestroyMesh(Graphics::MeshHandle mesh)
{
    target.DestroyMesh(mesh);
}

void EdgeExtendingDevice::DrawMesh(Graphics::MeshHandle mesh, const Graphics::MeshPlacement& placement, const Graphics::Material& material)
{
    target.DrawMesh(mesh, placement, material);
}

Graphics::TextureHandle EdgeExtendingDevice::BeginOffscreen(const Graphics::Canvas& canvas, std::uint32_t width, std::uint32_t height)
{
    screenCanvasArea = canvasArea;
    screenScissor = scissor;
    canvasArea = AreaOfCanvas(canvas);
    scissor.reset();
    return target.BeginOffscreen(canvas, width, height);
}

void EdgeExtendingDevice::EndOffscreen()
{
    target.EndOffscreen();
    canvasArea = screenCanvasArea;
    scissor = screenScissor;
}

void EdgeExtendingDevice::Present()
{
    target.Present();
}

}
