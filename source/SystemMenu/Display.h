#pragma once

#include "SystemMenu/EdgeExtension.h"
#include "SystemMenu/ZoomedDevice.h"

#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"
#include "Layout/TextFonts.h"

#include <vector>

namespace SystemMenu
{

// A layout that was drawn into the picture, and how it was placed.
struct DrawnLayout
{
    const Layout::LiveLayout* layout = nullptr;
    Layout::PlacementSettings placement;
};

// The layouts of the menu are made for a 4:3 picture of 608 by 456 layout units. WeeOS always shows the 16:9 picture of 832 by 456 units
// (docs/DECISIONS.md, Settings), into which the layouts are stretched. Content that only exists in 4:3 lies in the middle of it.
constexpr Graphics::Canvas StandardCanvas = {608, 456};
constexpr Graphics::Canvas WideCanvas = {832, 456};

// The picture the menu draws into. On a screen of another shape the picture stays 16:9 in the middle, and the backgrounds go on to the
// edges of the screen (EdgeExtendingDevice), which every drawing through Device() gets.
class Display
{
public:
    explicit Display(Graphics::GraphicsDevice& graphicsDevice);

    // The device to draw with. While an area is moved, the device moves what is drawn. Whatever draws for the display keeps this
    // device, also the text of the layouts, so that it is moved too.
    Graphics::GraphicsDevice& Device();

    // Everything that is drawn from now on into seenArea lands in shownArea, with its alpha scaled by alpha / 255, until StopMovingAreas.
    void MoveArea(const PictureArea& seenArea, const PictureArea& shownArea, std::uint8_t alpha);
    void StopMovingAreas();

    // The fonts that the text boxes of the layouts are drawn with. Without fonts the text boxes draw nothing.
    // The fonts must stay alive as long as the display draws.
    void SetTextFonts(const Layout::TextFonts& fonts);
    const Layout::TextFonts* TextFonts() const;

    // The size of the picture in layout units.
    Graphics::Canvas GetCanvas() const;

    // The whole picture, in layout units.
    PictureArea WholePicture() const;

    // Paints a rectangle of the picture in one color.
    void FillArea(const PictureArea& area, Formats::Rgba8Color color);

    // Starts a new picture of the screen. The program shows it with Present when everything is drawn.
    void BeginPicture();

    // How much a layout is stretched sideways to fill the picture. The layouts are 608 by 456 units, so it is (832 / 608, 1).
    // Panes that adjust their location are squeezed back by the opposite, so they keep their shape.
    Formats::Vector2 LayoutStretch() const;

    // A place in the pointer space of the Input part (the middle of the picture is (0, 0), the edges are -1 and 1, y grows upward)
    // as a place in the picture, in layout units.
    Formats::Vector2 PointerToCanvas(Formats::Vector2 pointer) const;

    // A place in the picture, as it is written into a pane that sits right below the root pane of a stretched layout.
    // The root pane stretches the place, so the pane gets the place divided by the stretch.
    Formats::Vector2 CanvasToRootPaneSpace(Formats::Vector2 canvasPoint) const;

    // The layouts that were drawn since the scene began, the one at the back first. A test finds the panes of the picture here.
    // They are valid until the menu takes its next step.
    void NoteDrawnLayout(const Layout::LiveLayout& layout, const Layout::PlacementSettings& placement);
    void ForgetDrawnLayouts();
    const std::vector<DrawnLayout>& DrawnLayouts() const;

private:
    EdgeExtendingDevice edgeDevice;
    ZoomedDevice zoomedDevice;
    const Layout::TextFonts* textFonts = nullptr;
    std::vector<DrawnLayout> drawnLayouts;
};

}
