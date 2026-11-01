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

constexpr PageTextStyle HeaderStyle = {22, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle EditHeaderStyle = {22, {255, 255, 255, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle TileTitleStyle = {16, {0, 0, 0, 255}, Text::HorizontalAlignment::Center};
constexpr PageTextStyle EmptyStyle = {18, {0, 0, 0, 255}, Text::HorizontalAlignment::Center};

// .preedit of favorites.css: the band of the title in edit mode.
constexpr Formats::Rgba8Color EditBandColor = {0x9a, 0x9f, 0xa3, 255};
constexpr Formats::Rgba8Color PageColor = {255, 255, 255, 255};

constexpr std::string_view FavoritesTitle = "S_FAVS";
constexpr std::string_view EditTitle = "S_EDIT_FAV";

// "There are currently no favorites" is asked for by its id.
constexpr std::int32_t NoFavoritesTextId = -1773677501;

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
{
}

void FavoritesScreen::Open()
{
    isEditing = false;
    firstRow = 0;
}

bool FavoritesScreen::IsEditing() const
{
    return isEditing;
}

void FavoritesScreen::SetEditing(bool shouldEdit)
{
    isEditing = shouldEdit;
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

void FavoritesScreen::Draw(PagePainter& painter, const PageFrame& frame, const std::vector<Favorite>& favorites,
    std::optional<std::size_t> focusedIndex) const
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
    {
        PageRectangle place = TileOf(index, frame);
        bool isVisible = place.top >= TreeTop && place.top + TileHeight <= frame.height - PageToolbar::Height;
        if (!isVisible)
            continue;

        float stateTop = focusedIndex == index ? TileStateDistance : 0;
        painter.DrawPicturePart(*tile, place, {0, stateTop, TileWidth, TileHeight});
        painter.DrawPicture(*thumbnail, {place.left + ThumbnailPlace.left, place.top + ThumbnailPlace.top, ThumbnailPlace.width, ThumbnailPlace.height});
        if (isEditing)
            painter.DrawPicture(*editFade, {place.left, place.top, editFade->Width(), editFade->Height()});
        painter.DrawTextLine(favorites[index].title, TileTitleStyle,
            {place.left + TileTitleBox.left, place.top + TileTitleBox.top, TileTitleBox.width, TileTitleBox.height});
    }
}

PageRectangle FavoritesScreen::TileOf(std::size_t index, const PageFrame& frame) const
{
    float treeLeft = (frame.width - TreeWidth) / 2;
    int row = static_cast<int>(index / TilesPerRow) - firstRow;
    int column = static_cast<int>(index % TilesPerRow);
    return {treeLeft + column * TileWidth, TreeTop + row * TileHeight, TileWidth, TileHeight};
}

}
