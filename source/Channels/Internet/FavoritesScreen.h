#pragma once

#include "Channels/Internet/OperaData.h"
#include "Channels/OperaLanguageFile.h"
#include "Channels/PagePainter.h"

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"

#include <memory>
#include <optional>
#include <vector>

namespace Channels
{

// The favorites of the Internet Channel (favorites.html with the widescreen favorites.css, the manual page 5): the title "Favorites",
// then the favorites as tiles of 135 by 110 pixels, four in a row, each with its picture and its title. In edit mode the title is
// "Edit Favorites" and a favorite that is pressed is offered for deletion. The toolbar of the screen belongs to the channel.
class FavoritesScreen
{
public:
    static constexpr int TilesPerRow = 4;

    // The device, the pages and the texts must outlive the screen.
    FavoritesScreen(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, const OperaLanguageFile& texts);

    void Open();
    bool IsEditing() const;
    void SetEditing(bool isEditing);

    // Moves the list by whole rows; it stops at the first row and at the last.
    void ScrollRows(int rows, std::size_t favoriteCount);

    // The index of the favorite at the point of the page, in the list that is drawn.
    std::optional<std::size_t> FavoriteAt(Formats::Vector2 pagePoint, const PageFrame& frame, std::size_t favoriteCount) const;

    // The page above the toolbar.
    void Draw(PagePainter& painter, const PageFrame& frame, const std::vector<Favorite>& favorites, std::optional<std::size_t> focusedIndex) const;

private:
    PageRectangle TileOf(std::size_t index, const PageFrame& frame) const;
    int VisibleRowCount(const PageFrame& frame) const;

    const OperaLanguageFile& texts;
    std::unique_ptr<PagePicture> tile;
    std::unique_ptr<PagePicture> thumbnail;
    std::unique_ptr<PagePicture> editFade;
    bool isEditing = false;
    int firstRow = 0;
};

}
