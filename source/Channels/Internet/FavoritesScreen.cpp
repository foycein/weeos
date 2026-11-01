#include "Channels/Internet/FavoritesScreen.h"

#include "Channels/ChannelFiles.h"
#include "Channels/Internet/PageToolbar.h"

#include "Formats/PngImage.h"

#include <algorithm>

namespace Channels
{

namespace
{

constexpr std::string_view PictureFolder = "./favorites/stylesWideScreen/";

// The places of the widescreen favorites.css: the title, and the tree of tiles 540 pixels wide in the middle of the page.
constexpr float TreeWidth = 540;
constexpr float HeaderTop = 22;
constexpr float HeaderHeight = 33;
constexpr float HeaderIndent = 6;
constexpr float TreeTop = HeaderTop + HeaderHeight;
constexpr float TileWidth = 135;
constexpr float TileHeight = 110;
constexpr PageRectangle ThumbnailPlace = {22, 13, 90, 67};
constexpr PageRectangle TileTitleBox = {15, 82, 105, 22};

// favoriteBG.png holds the tile as it is and the tile that is pointed at, one under the other (Assumed: 115 pixels apart, a third of it).
constexpr float TileStateDistance = 115;

// #editOptionDialog of favorites.css: a menu 203 pixels high whose right end is the last 25 pixels of its picture, rows of 42 pixels with
// 2 between them from 12 pixels below the top and 9 from the left, the icon 15 pixels into the row and the text 37 pixels after it. The
// rows show editOptionHighlight.png as it is (top half) or pointed at (bottom half), with its last 21 pixels as the right end.
constexpr float EditMenuWidth = 200;
constexpr float EditMenuHeight = 203;
constexpr float EditMenuRightEndWidth = 25;
constexpr float EditRowsTop = 12;
constexpr float EditRowsLeft = 9;
constexpr float EditRowHeight = 42;
constexpr float EditRowGap = 2;
constexpr float EditRowRightEndWidth = 21;
constexpr float EditIconLeft = 15;
constexpr float EditIconWidth = 30;
constexpr float EditTextLeft = EditIconLeft + 37;
constexpr float EditTextTop = 11;
constexpr float EditMenuGap = 4;

// A row that cannot be chosen is shown faint (#editOptionDialog li.disabled, opacity .35).
constexpr Formats::Rgba8Color FaintRowCover = {255, 255, 255, 166};

// .arrow of favorites.css: 37 pixels; they stand on the four sides of the moving favorite (Assumed: their places come from a script).
constexpr float ArrowSize = 37;

constexpr PageTextStyle HeaderStyle = {22, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle EditHeaderStyle = {22, {255, 255, 255, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle TileTitleStyle = {16, {0, 0, 0, 255}, Text::HorizontalAlignment::Center};
constexpr PageTextStyle EmptyStyle = {18, {0, 0, 0, 255}, Text::HorizontalAlignment::Center};
constexpr PageTextStyle EditRowStyle = {18, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};

// .preedit of favorites.css: the band of the title in edit mode.
constexpr Formats::Rgba8Color EditBandColor = {0x9a, 0x9f, 0xa3, 255};
constexpr Formats::Rgba8Color PageColor = {255, 255, 255, 255};

constexpr std::string_view FavoritesTitle = "S_FAVS";
constexpr std::string_view EditTitle = "S_EDIT_FAV";

// "There are currently no favorites" is asked for by its id.
constexpr std::int32_t NoFavoritesTextId = -1773677501;

// The rows of the edit options in their order, with their texts (uiStrings.js) and icons (favorites.css).
constexpr std::array<std::string_view, 4> EditOptionTexts = {
    "S_FAVORITES_BUTTON_DELETE", "S_FAVORITES_BUTTON_RENAME", "S_FAVORITES_BUTTON_MOVE", "DI_OPERA_QT_BUTTON_CLOSE"};
constexpr std::array<std::string_view, 4> EditOptionIcons = {"trashIcon.png", "editIcon.png", "moveIcon.png", "closeIcon.png"};

// The arrows around a moving favorite: up, right, down, left.
constexpr std::array<std::string_view, 4> MoveArrowFiles = {"moveArrowUp.png", "moveArrowRight.png", "moveArrowDown.png", "moveArrowLeft.png"};

std::unique_ptr<PagePicture> ReadPicture(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, std::string_view name)
{
    std::string path = std::string(PictureFolder) + std::string(name);
    return std::make_unique<PagePicture>(device, Formats::ReadPngImage(FindFileOrFail(pages, path)));
}

}

FavoritesScreen::FavoritesScreen(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, const OperaLanguageFile& languageTexts)
    : texts(languageTexts)
    , tile(ReadPicture(device, pages, "favoriteBG.png"))
    , thumbnail(ReadPicture(device, pages, "fallbackThumbnailFavorites.png"))
    , editFade(ReadPicture(device, pages, "fadeBG.png"))
    , editBackground(ReadPicture(device, pages, "itemEditBackground.png"))
    , editHighlight(ReadPicture(device, pages, "editOptionHighlight.png"))
{
    for (std::size_t index = 0; index < editIcons.size(); ++index)
        editIcons[index] = ReadPicture(device, pages, EditOptionIcons[index]);
    for (std::size_t index = 0; index < moveArrows.size(); ++index)
        moveArrows[index] = ReadPicture(device, pages, MoveArrowFiles[index]);
}

void FavoritesScreen::Open()
{
    isEditing = false;
    firstRow = 0;
    editedFavorite.reset();
    movingFavorite.reset();
}

bool FavoritesScreen::IsEditing() const
{
    return isEditing;
}

void FavoritesScreen::SetEditing(bool shouldEdit)
{
    isEditing = shouldEdit;
    editedFavorite.reset();
    movingFavorite.reset();
}

void FavoritesScreen::SetThumbnails(std::map<int, std::unique_ptr<PagePicture>> thumbnails)
{
    favoriteThumbnails = std::move(thumbnails);
}

void FavoritesScreen::ScrollRows(int rows, std::size_t favoriteCount)
{
    int rowCount = static_cast<int>((favoriteCount + TilesPerRow - 1) / TilesPerRow);
    firstRow = std::clamp(firstRow + rows, 0, std::max(0, rowCount - 1));
}

std::optional<std::size_t> FavoritesScreen::FavoriteAt(Formats::Vector2 pagePoint, const PageFrame& frame, std::size_t favoriteCount) const
{
    if (pagePoint.y >= frame.height - PageToolbar::Height)
        return std::nullopt;

    for (std::size_t index = 0; index < favoriteCount; ++index)
    {
        if (TileOf(index, frame).Contains(pagePoint))
            return index;
    }
    return std::nullopt;
}

void FavoritesScreen::OpenEditOptions(std::size_t favoriteIndex)
{
    editedFavorite = favoriteIndex;
}

void FavoritesScreen::CloseEditOptions()
{
    editedFavorite.reset();
}

std::optional<std::size_t> FavoritesScreen::EditedFavorite() const
{
    return editedFavorite;
}

std::optional<FavoritesScreen::EditOption> FavoritesScreen::EditOptionAt(Formats::Vector2 pagePoint, const PageFrame& frame,
    std::size_t favoriteCount) const
{
    if (!editedFavorite)
        return std::nullopt;

    PageRectangle menu = EditMenuOf(*editedFavorite, frame);
    for (std::size_t row = 0; row < EditOptionTexts.size(); ++row)
    {
        PageRectangle rowPlace = {menu.left + EditRowsLeft, menu.top + EditRowsTop + row * (EditRowHeight + EditRowGap),
            menu.width - 2 * EditRowsLeft, EditRowHeight};
        if (!rowPlace.Contains(pagePoint))
            continue;

        EditOption option = static_cast<EditOption>(row);
        bool canMove = favoriteCount > 1;
        if (option == EditOption::Move && !canMove)
            return std::nullopt;
        return option;
    }
    return std::nullopt;
}

void FavoritesScreen::StartMoving(std::size_t favoriteIndex)
{
    editedFavorite.reset();
    movingFavorite = favoriteIndex;
}

void FavoritesScreen::StopMoving()
{
    movingFavorite.reset();
}

std::optional<std::size_t> FavoritesScreen::MovingFavorite() const
{
    return movingFavorite;
}

void FavoritesScreen::SetMovingFavorite(std::size_t favoriteIndex)
{
    movingFavorite = favoriteIndex;
}

void FavoritesScreen::Draw(PagePainter& painter, const PageFrame& frame, const std::vector<Favorite>& favorites,
    std::optional<std::size_t> focusedIndex, std::optional<EditOption> focusedOption) const
{
    painter.FillRectangle({0, 0, frame.width, frame.height}, PageColor);

    float treeLeft = (frame.width - TreeWidth) / 2;
    PageRectangle headerBox = {treeLeft + HeaderIndent, HeaderTop, TreeWidth, HeaderHeight};
    if (isEditing)
    {
        painter.FillRectangle({0, 0, frame.width, TreeTop}, EditBandColor);
        painter.DrawText(texts.Find(EditTitle), EditHeaderStyle, headerBox);
    }
    else
    {
        painter.DrawText(texts.Find(FavoritesTitle), HeaderStyle, headerBox);
    }

    if (favorites.empty())
    {
        painter.DrawText(texts.FindById(NoFavoritesTextId), EmptyStyle, {treeLeft, TreeTop + TileHeight, TreeWidth, TileHeight});
        return;
    }

    for (std::size_t index = 0; index < favorites.size(); ++index)
        DrawTile(painter, frame, favorites[index], index, focusedIndex == index || movingFavorite == index);

    if (editedFavorite)
        DrawEditMenu(painter, frame, favorites.size(), focusedOption);
    if (movingFavorite)
        DrawMoveArrows(painter, frame);
}

PageRectangle FavoritesScreen::TileOf(std::size_t index, const PageFrame& frame) const
{
    float treeLeft = (frame.width - TreeWidth) / 2;
    int row = static_cast<int>(index / TilesPerRow) - firstRow;
    int column = static_cast<int>(index % TilesPerRow);
    return {treeLeft + column * TileWidth, TreeTop + row * TileHeight, TileWidth, TileHeight};
}

// The menu stands beside the favorite on the right, or on the left when there is no room on the right.
PageRectangle FavoritesScreen::EditMenuOf(std::size_t index, const PageFrame& frame) const
{
    PageRectangle tilePlace = TileOf(index, frame);
    float left = tilePlace.left + tilePlace.width + EditMenuGap;
    if (left + EditMenuWidth > frame.width)
        left = tilePlace.left - EditMenuGap - EditMenuWidth;

    float lowestTop = frame.height - PageToolbar::Height - EditMenuHeight;
    float top = std::clamp(tilePlace.top, TreeTop, lowestTop);
    return {left, top, EditMenuWidth, EditMenuHeight};
}

void FavoritesScreen::DrawTile(PagePainter& painter, const PageFrame& frame, const Favorite& favorite, std::size_t index, bool isFocused) const
{
    PageRectangle place = TileOf(index, frame);
    bool isVisible = place.top >= TreeTop && place.top + TileHeight <= frame.height - PageToolbar::Height;
    if (!isVisible)
        return;

    float stateTop = isFocused ? TileStateDistance : 0;
    painter.DrawPicturePart(*tile, place, {0, stateTop, TileWidth, TileHeight});

    auto stored = favoriteThumbnails.find(favorite.number);
    const PagePicture& picture = stored != favoriteThumbnails.end() ? *stored->second : *thumbnail;
    painter.DrawPicture(picture, {place.left + ThumbnailPlace.left, place.top + ThumbnailPlace.top, ThumbnailPlace.width, ThumbnailPlace.height});

    if (isEditing)
        painter.DrawPicture(*editFade, {place.left, place.top, editFade->Width(), editFade->Height()});
    painter.DrawTextLine(favorite.title, TileTitleStyle,
        {place.left + TileTitleBox.left, place.top + TileTitleBox.top, TileTitleBox.width, TileTitleBox.height});
}

void FavoritesScreen::DrawEditMenu(PagePainter& painter, const PageFrame& frame, std::size_t favoriteCount,
    std::optional<EditOption> focusedOption) const
{
    PageRectangle menu = EditMenuOf(*editedFavorite, frame);
    float bodyWidth = menu.width - EditMenuRightEndWidth;
    painter.DrawPicturePart(*editBackground, {menu.left, menu.top, bodyWidth, menu.height}, {0, 0, bodyWidth, menu.height});
    painter.DrawPicturePart(*editBackground, {menu.left + bodyWidth, menu.top, EditMenuRightEndWidth, menu.height},
        {editBackground->Width() - EditMenuRightEndWidth, 0, EditMenuRightEndWidth, menu.height});

    for (std::size_t row = 0; row < EditOptionTexts.size(); ++row)
    {
        PageRectangle rowPlace = {menu.left + EditRowsLeft, menu.top + EditRowsTop + row * (EditRowHeight + EditRowGap),
            menu.width - 2 * EditRowsLeft, EditRowHeight};
        bool isFocused = focusedOption == static_cast<EditOption>(row);
        float stateTop = isFocused ? EditRowHeight : 0;
        float rowBodyWidth = rowPlace.width - EditRowRightEndWidth;
        painter.DrawPicturePart(*editHighlight, {rowPlace.left, rowPlace.top, rowBodyWidth, EditRowHeight}, {0, stateTop, rowBodyWidth, EditRowHeight});
        painter.DrawPicturePart(*editHighlight, {rowPlace.left + rowBodyWidth, rowPlace.top, EditRowRightEndWidth, EditRowHeight},
            {editHighlight->Width() - EditRowRightEndWidth, stateTop, EditRowRightEndWidth, EditRowHeight});
        painter.DrawPicturePart(*editIcons[row], {rowPlace.left + EditIconLeft, rowPlace.top, EditIconWidth, EditRowHeight},
            {0, stateTop, EditIconWidth, EditRowHeight});
        painter.DrawText(texts.Find(EditOptionTexts[row]), EditRowStyle,
            {rowPlace.left + EditTextLeft, rowPlace.top + EditTextTop, rowPlace.width - EditTextLeft, EditRowHeight});

        bool isFaint = static_cast<EditOption>(row) == EditOption::Move && favoriteCount < 2;
        if (isFaint)
            painter.FillRectangle(rowPlace, FaintRowCover);
    }
}

void FavoritesScreen::DrawMoveArrows(PagePainter& painter, const PageFrame& frame) const
{
    PageRectangle place = TileOf(*movingFavorite, frame);
    float middleX = place.left + place.width / 2 - ArrowSize / 2;
    float middleY = place.top + place.height / 2 - ArrowSize / 2;
    painter.DrawPicture(*moveArrows[0], {middleX, place.top - ArrowSize / 2, ArrowSize, ArrowSize});
    painter.DrawPicture(*moveArrows[1], {place.left + place.width - ArrowSize / 2, middleY, ArrowSize, ArrowSize});
    painter.DrawPicture(*moveArrows[2], {middleX, place.top + place.height - ArrowSize / 2, ArrowSize, ArrowSize});
    painter.DrawPicture(*moveArrows[3], {place.left - ArrowSize / 2, middleY, ArrowSize, ArrowSize});
}

}
