#include "SystemMenu/WiiMenu/BarHousings.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Layout/LayoutDrawer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

namespace
{

// Every housing is three pictures: an end, a middle piece and the other end ("<housing>_0", "_1", "_2" of my_IplTop_e). a0 and a1 hold
// the Wii button and the envelope; the others belong to the buttons of the Message Board.
constexpr std::array<std::string_view, 10> HousingsOfTheButtons = {
    "BtnL_a0", "BtnL_a1", "BtnL_a3", "BtnL_a4", "BtnL_a5", "BtnL_a6", "BtnL_a7", "BtnL_a8", "BtnR_a0", "BtnR_a1"};
constexpr std::string_view FirstEnd = "_0";
constexpr std::string_view MiddlePiece = "_1";
constexpr std::string_view SecondEnd = "_2";

// A screen this little wider than the picture shows nothing beside it.
constexpr float SideTolerance = 0.5f;

enum class Side
{
    Left,
    Right,
};

// A housing that was stretched for one picture, with what its panes were before.
struct StretchedHousing
{
    Formats::Pane* middle = nullptr;
    Formats::Pane* outerEnd = nullptr;
    float middleWidth = 0;
    float middleX = 0;
    float outerEndX = 0;
};

std::string PieceName(std::string_view housing, std::string_view piece)
{
    return std::string(housing) + std::string(piece);
}

// Grows the middle piece outward by a number of its own units and moves the outer end out with it. The outer end lies on the outward
// side of the middle piece in their parent as well, which may be drawn mirrored.
StretchedHousing Stretch(Formats::Pane& middle, Formats::Pane& outerEnd, float growth)
{
    StretchedHousing stretched = {&middle, &outerEnd, middle.width, middle.translation.x, outerEnd.translation.x};

    float outward = outerEnd.translation.x < middle.translation.x ? -1.0f : 1.0f;
    float growthInParent = growth * middle.scale.x;
    middle.width += growth;
    middle.translation.x += outward * growthInParent / 2;
    outerEnd.translation.x += outward * growthInParent;
    return stretched;
}

void PutBack(const StretchedHousing& stretched)
{
    stretched.middle->width = stretched.middleWidth;
    stretched.middle->translation.x = stretched.middleX;
    stretched.outerEnd->translation.x = stretched.outerEndX;
}

class HousingStretcher
{
public:
    HousingStretcher(MenuLayout& barLayout, const Graphics::ScissorRectangle& canvas, const Graphics::ScissorRectangle& screen)
        : layout(barLayout)
        , placedPanes(barLayout)
        , canvasArea(canvas)
        , screenArea(screen)
    {
    }

    std::optional<StretchedHousing> StretchToTheSide(std::string_view housing)
    {
        std::string middleName = PieceName(housing, MiddlePiece);
        std::optional<HorizontalSpan> middleSpan = placedPanes.SpanOf(middleName);
        if (!middleSpan)
            return std::nullopt;

        Formats::Pane& middle = layout.PaneNamed(middleName);
        float pictureUnitsPerPaneUnit = (middleSpan->right - middleSpan->left) / middle.width;

        bool crossesTheLeftSide = middleSpan->left < canvasArea.left && middleSpan->right > canvasArea.left;
        if (crossesTheLeftSide)
        {
            float growth = (middleSpan->left - screenArea.left) / pictureUnitsPerPaneUnit;
            return Stretch(middle, OuterEnd(housing, Side::Left), growth);
        }

        bool crossesTheRightSide = middleSpan->left < canvasArea.right && middleSpan->right > canvasArea.right;
        if (crossesTheRightSide)
        {
            float growth = (screenArea.right - middleSpan->right) / pictureUnitsPerPaneUnit;
            return Stretch(middle, OuterEnd(housing, Side::Right), growth);
        }
        return std::nullopt;
    }

private:
    // The end that lies further toward the side in the picture; the housings of the left side are drawn mirrored, so the name does
    // not say which one it is.
    Formats::Pane& OuterEnd(std::string_view housing, Side side)
    {
        std::string firstName = PieceName(housing, FirstEnd);
        std::string secondName = PieceName(housing, SecondEnd);
        float firstX = placedPanes.PlaceOf(firstName).value_or(Formats::Vector2{}).x;
        float secondX = placedPanes.PlaceOf(secondName).value_or(Formats::Vector2{}).x;

        bool firstIsLeftOfSecond = firstX < secondX;
        bool firstIsOuter = side == Side::Left ? firstIsLeftOfSecond : !firstIsLeftOfSecond;
        return layout.PaneNamed(firstIsOuter ? firstName : secondName);
    }

    MenuLayout& layout;
    PlacedPanes placedPanes;
    Graphics::ScissorRectangle canvasArea;
    Graphics::ScissorRectangle screenArea;
};

Graphics::ScissorRectangle AreaOfCanvas(const Graphics::Canvas& canvas)
{
    return {-canvas.width / 2, canvas.width / 2, -canvas.height / 2, canvas.height / 2};
}

bool IsStretched(const std::vector<StretchedHousing>& stretchedHousings, const Formats::Pane& pane)
{
    return std::any_of(stretchedHousings.begin(), stretchedHousings.end(),
        [&pane](const StretchedHousing& housing) { return housing.middle == &pane || housing.outerEnd == &pane; });
}

std::vector<StretchedHousing> StretchTheHousingsToTheSides(MenuLayout& layout, const Graphics::ScissorRectangle& canvasArea,
    const Graphics::ScissorRectangle& screenArea)
{
    std::vector<StretchedHousing> stretchedHousings;
    HousingStretcher stretcher(layout, canvasArea, screenArea);
    for (std::string_view housing : HousingsOfTheButtons)
    {
        std::optional<StretchedHousing> stretched = stretcher.StretchToTheSide(housing);
        if (stretched)
            stretchedHousings.push_back(*stretched);
    }
    return stretchedHousings;
}

// Only the stretched housings reach beside the picture, and only sideways.
void DrawWithTheStretchedHousings(MenuLayout& layout, Display& display, const std::vector<StretchedHousing>& stretchedHousings,
    const Graphics::ScissorRectangle& sidesOfTheScreen)
{
    Graphics::GraphicsDevice& device = display.Device();
    PlacedPanes placedPanes(layout);
    for (const Layout::PanePlacement& placement : placedPanes.InDrawingOrder())
    {
        bool reachesBeside = IsStretched(stretchedHousings, *placement.pane);
        if (reachesBeside)
            device.SetScissor(sidesOfTheScreen);
        Layout::DrawPlacedPane(device, layout.Live(), placement, display.TextFonts());
        if (reachesBeside)
            device.ClearScissor();
    }
    display.NoteDrawnLayout(layout.Live(), layout.Placement());
}

}

void DrawBarWithHousingsToTheSides(MenuLayout& layout, Display& display)
{
    Graphics::ScissorRectangle canvasArea = AreaOfCanvas(display.GetCanvas());
    Graphics::ScissorRectangle screenArea = display.Device().WholeArea();
    bool reachesBeyondTheLeft = screenArea.left < canvasArea.left - SideTolerance;
    bool reachesBeyondTheRight = screenArea.right > canvasArea.right + SideTolerance;
    if (!reachesBeyondTheLeft && !reachesBeyondTheRight)
    {
        layout.Draw();
        return;
    }

    std::vector<StretchedHousing> stretchedHousings = StretchTheHousingsToTheSides(layout, canvasArea, screenArea);
    Graphics::ScissorRectangle sidesOfTheScreen = {screenArea.left, screenArea.right, canvasArea.bottom, canvasArea.top};
    DrawWithTheStretchedHousings(layout, display, stretchedHousings, sidesOfTheScreen);

    for (const StretchedHousing& stretched : stretchedHousings)
        PutBack(stretched);
}

}
