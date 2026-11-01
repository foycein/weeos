#pragma once

#include "Channels/PagePainter.h"

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace Channels
{

enum class ToolbarButton
{
    Back,
    Forward,

    // Shows Stop while the page loads.
    Reload,

    Search,
    Favorites,
    WebAddress,
    StartPage,

    // The page information, shown as a lock for a secure page.
    PageInfo,

    AddFavorite,
    EditFavorites,
    Close,
};

// What the toolbar shows next to its buttons.
struct ToolbarState
{
    std::optional<ToolbarButton> focusedButton;
    std::set<ToolbarButton> disabledButtons;
    bool isLoading = false;
    bool isSecure = false;

    // From 0 to 1 while a page loads.
    double loadingProgress = 0;
};

// The bar at the bottom of the browser (the manual, page 2, "Toolbar"): a light gray band with the buttons of the screen side by side
// in the middle. The buttons are the pictures of cursor.tpl in content 11 of the channel, three for each button: as it is, pointed at
// and unusable.
class PageToolbar
{
public:
    static constexpr float Height = 70;

    // The device and the files must outlive the toolbar.
    PageToolbar(Graphics::GraphicsDevice& device, const Formats::U8Archive& toolbarFiles);

    // The buttons of a screen, from left to right.
    void ShowButtons(const std::vector<ToolbarButton>& buttons);

    // The toolbar lies at the bottom of a page as wide as the picture.
    std::optional<ToolbarButton> ButtonAt(Formats::Vector2 pagePoint, const PageFrame& frame) const;
    void Draw(PagePainter& painter, const PageFrame& frame, const ToolbarState& state) const;

private:
    struct PlacedButton
    {
        ToolbarButton button;
        PageRectangle place;
    };

    std::vector<PlacedButton> PlaceButtons(const PageFrame& frame) const;
    const PagePicture& PictureOf(ToolbarButton button, const ToolbarState& state) const;

    std::vector<std::unique_ptr<PagePicture>> pictures;
    std::vector<ToolbarButton> shownButtons;
};

}
