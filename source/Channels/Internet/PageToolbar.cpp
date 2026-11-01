#include "Channels/Internet/PageToolbar.h"

#include "Channels/ChannelFiles.h"

#include "Formats/TplTexture.h"

namespace Channels
{

namespace
{

constexpr std::string_view ToolbarPicturesPath = "cursor.tpl";

// The pictures of cursor.tpl. Every button has three in a row: as it is, pointed at, unusable.
constexpr int BackPictures = 0;
constexpr int ForwardPictures = 3;
constexpr int ReloadPictures = 6;
constexpr int StopPictures = 9;
constexpr int FavoritesPictures = 12;
constexpr int StartPagePictures = 15;
constexpr int WebAddressPictures = 18;
constexpr int SearchPictures = 21;
constexpr int ClosePictures = 30;
constexpr int AddFavoritePictures = 36;
constexpr int EditFavoritesPictures = 39;
constexpr int PointedAtOffset = 1;
constexpr int UnusableOffset = 2;

// The lock and the sign of the page information have two pictures: usable and unusable.
constexpr int LockPicture = 52;
constexpr int InformationPicture = 54;

// The blue bar of the loading progress, 800 by 8 pixels.
constexpr int ProgressBarPicture = 51;
constexpr float ProgressBarHeight = 8;

// The places of the buttons (Assumed: drawn after the picture of the toolbar in the manual, page 2, at the size of the button pictures).
// Back and Forward touch each other; the page information keeps a small distance on the right.
constexpr float ButtonTop = 2;
constexpr float GapAfterForward = 4;
constexpr float GapBeforePageInfo = 8;
constexpr float PageInfoTop = 20;

// The colors of the band: light gray with a white line on top (the manual's picture of the toolbar).
constexpr Formats::Rgba8Color BandColor = {227, 227, 227, 255};
constexpr Formats::Rgba8Color TopLineColor = {255, 255, 255, 255};
constexpr float TopLineHeight = 1;

int FirstPictureOf(ToolbarButton button, bool isLoading)
{
    switch (button)
    {
    case ToolbarButton::Back:
        return BackPictures;
    case ToolbarButton::Forward:
        return ForwardPictures;
    case ToolbarButton::Reload:
        return isLoading ? StopPictures : ReloadPictures;
    case ToolbarButton::Search:
        return SearchPictures;
    case ToolbarButton::Favorites:
        return FavoritesPictures;
    case ToolbarButton::WebAddress:
        return WebAddressPictures;
    case ToolbarButton::StartPage:
        return StartPagePictures;
    case ToolbarButton::PageInfo:
        return InformationPicture;
    case ToolbarButton::AddFavorite:
        return AddFavoritePictures;
    case ToolbarButton::EditFavorites:
        return EditFavoritesPictures;
    case ToolbarButton::Close:
        return ClosePictures;
    }
    return ClosePictures;
}

}

PageToolbar::PageToolbar(Graphics::GraphicsDevice& device, const Formats::U8Archive& toolbarFiles)
{
    for (const Formats::TplTexture& texture : Formats::ReadTplTextures(FindFileOrFail(toolbarFiles, ToolbarPicturesPath)))
        pictures.push_back(std::make_unique<PagePicture>(device, Formats::DecodeTexture(texture)));
}

void PageToolbar::ShowButtons(const std::vector<ToolbarButton>& buttons)
{
    shownButtons = buttons;
}

std::optional<ToolbarButton> PageToolbar::ButtonAt(Formats::Vector2 pagePoint, const PageFrame& frame) const
{
    for (const PlacedButton& placed : PlaceButtons(frame))
    {
        if (placed.place.Contains(pagePoint))
            return placed.button;
    }
    return std::nullopt;
}

void PageToolbar::Draw(PagePainter& painter, const PageFrame& frame, const ToolbarState& state) const
{
    float top = frame.height - Height;
    painter.FillRectangle({0, top, frame.width, Height}, BandColor);
    painter.FillRectangle({0, top, frame.width, TopLineHeight}, TopLineColor);

    if (state.isLoading)
    {
        const PagePicture& bar = *pictures[ProgressBarPicture];
        float width = static_cast<float>(frame.width * state.loadingProgress);
        painter.DrawPicturePart(bar, {0, top, width, ProgressBarHeight}, {0, 0, width, ProgressBarHeight});
    }

    for (const PlacedButton& placed : PlaceButtons(frame))
        painter.DrawPicture(PictureOf(placed.button, state), placed.place);
}

std::vector<PageToolbar::PlacedButton> PageToolbar::PlaceButtons(const PageFrame& frame) const
{
    std::vector<PlacedButton> placed;
    float totalWidth = 0;
    for (ToolbarButton button : shownButtons)
    {
        const PagePicture& picture = *pictures[FirstPictureOf(button, false)];
        if (button == ToolbarButton::PageInfo)
            totalWidth += GapBeforePageInfo;
        placed.push_back({button, {totalWidth, ButtonTop, picture.Width(), picture.Height()}});
        totalWidth += picture.Width();
        if (button == ToolbarButton::Forward)
            totalWidth += GapAfterForward;
    }

    float left = (frame.width - totalWidth) / 2;
    float top = frame.height - Height;
    for (PlacedButton& button : placed)
    {
        button.place.left += left;
        button.place.top += top;
        if (button.button == ToolbarButton::PageInfo)
            button.place.top = top + PageInfoTop;
    }
    return placed;
}

const PagePicture& PageToolbar::PictureOf(ToolbarButton button, const ToolbarState& state) const
{
    bool isUnusable = state.disabledButtons.contains(button);
    if (button == ToolbarButton::PageInfo)
    {
        int picture = state.isSecure ? LockPicture : InformationPicture;
        return *pictures[isUnusable ? picture + 1 : picture];
    }

    int first = FirstPictureOf(button, state.isLoading);
    if (isUnusable)
        return *pictures[first + UnusableOffset];
    if (state.focusedButton == button)
        return *pictures[first + PointedAtOffset];
    return *pictures[first];
}

}
