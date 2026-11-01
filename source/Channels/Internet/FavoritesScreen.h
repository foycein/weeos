#pragma once

#include "Channels/Internet/OperaData.h"
#include "Channels/OperaLanguageFile.h"
#include "Channels/PagePainter.h"

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace Channels
{

// The favorites of the Internet Channel (favorites.html with the widescreen favorites.css, the manual page 5): the title "Favorites",
// then the favorites as tiles of 135 by 110 pixels, four in a row, each with its picture and its title. In edit mode the title is
// "Edit Favorites" and a favorite that is pressed opens the menu of the original's edit options beside it: Delete, Edit Title, Move and
// Close. While a favorite moves, arrows stand around it and the +Control Pad moves it. The toolbar of the screen belongs to the channel.
class FavoritesScreen
{
public:
    static constexpr int TilesPerRow = 4;

    // The rows of the edit options, from the top (templates.editOptionDialog).
    enum class EditOption
    {
        Delete,
        Rename,
        Move,
        Close,
    };

    // The device, the pages and the texts must outlive the screen.
    FavoritesScreen(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, const OperaLanguageFile& texts);

    void Open();
    bool IsEditing() const;
    void SetEditing(bool isEditing);

    // The pictures of the favorites by their numbers; a favorite without one shows the fallback picture.
    void SetThumbnails(std::map<int, std::unique_ptr<PagePicture>> thumbnails);

    // Moves the list by whole rows; it stops at the first row and at the last.
    void ScrollRows(int rows, std::size_t favoriteCount);

    // The index of the favorite at the point of the page, in the list that is drawn.
    std::optional<std::size_t> FavoriteAt(Formats::Vector2 pagePoint, const PageFrame& frame, std::size_t favoriteCount) const;

    // The menu of edit options of a favorite. Move is offered only when there is something to move it past.
    void OpenEditOptions(std::size_t favoriteIndex);
    void CloseEditOptions();
    std::optional<std::size_t> EditedFavorite() const;
    std::optional<EditOption> EditOptionAt(Formats::Vector2 pagePoint, const PageFrame& frame, std::size_t favoriteCount) const;

    void StartMoving(std::size_t favoriteIndex);
    void StopMoving();
    std::optional<std::size_t> MovingFavorite() const;
    void SetMovingFavorite(std::size_t favoriteIndex);

    // The page above the toolbar.
    void Draw(PagePainter& painter, const PageFrame& frame, const std::vector<Favorite>& favorites, std::optional<std::size_t> focusedIndex,
        std::optional<EditOption> focusedOption) const;

private:
    PageRectangle TileOf(std::size_t index, const PageFrame& frame) const;
    PageRectangle EditMenuOf(std::size_t index, const PageFrame& frame) const;
    void DrawTile(PagePainter& painter, const PageFrame& frame, const Favorite& favorite, std::size_t index, bool isFocused) const;
    void DrawEditMenu(PagePainter& painter, const PageFrame& frame, std::size_t favoriteCount, std::optional<EditOption> focusedOption) const;
    void DrawMoveArrows(PagePainter& painter, const PageFrame& frame) const;

    const OperaLanguageFile& texts;
    std::unique_ptr<PagePicture> tile;
    std::unique_ptr<PagePicture> thumbnail;
    std::unique_ptr<PagePicture> editFade;
    std::unique_ptr<PagePicture> editBackground;
    std::unique_ptr<PagePicture> editHighlight;
    std::array<std::unique_ptr<PagePicture>, 4> editIcons;
    std::array<std::unique_ptr<PagePicture>, 4> moveArrows;
    std::map<int, std::unique_ptr<PagePicture>> favoriteThumbnails;
    bool isEditing = false;
    int firstRow = 0;
    std::optional<std::size_t> editedFavorite;
    std::optional<std::size_t> movingFavorite;
};

}
