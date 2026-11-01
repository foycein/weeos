#include "SystemMenu/DataManagement/ChannelIconLayout.h"

#include "Formats/BannerContent.h"
#include "Layout/LayoutDrawer.h"
#include "Layout/LayoutError.h"

#include <array>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view IconLayoutName = "icon";
constexpr std::string_view IconAnimationPath = "arc/anim/icon.brlan";

// An icon or a banner has a group of panes for each language. The menu hides the groups of the other languages and shows the one of
// its own language (0x813B24C0, the names at 0x8164E300). WeeOS speaks English.
constexpr std::string_view EnglishGroup = "ENG";
constexpr std::array<std::string_view, 8> OtherLanguageGroups = {"JPN", "GER", "FRA", "SPA", "ITA", "NED", "CHN", "KOR"};

void SetGroupVisible(Layout::LiveLayout& layout, std::string_view groupName, bool isVisible)
{
    try
    {
        for (Formats::Pane* pane : layout.PanesOfGroup(groupName))
            pane->isVisible = isVisible;
    }
    catch (const Layout::LayoutError&)
    {
        // The icon has no panes of this language.
    }
}

// The layouts of the slots show this picture where the icon of the save data goes.
constexpr std::string_view EmptyPictureName = "it_DummyPicture_a.tpl";

}

ChannelIconLayout::ChannelIconLayout(Graphics::GraphicsDevice& graphicsDevice, Formats::ByteView iconFile)
    : device(graphicsDevice)
{
    Formats::U8Archive files = Formats::ReadLayoutFile(iconFile);
    bool hasAnimation = files.FindFile(IconAnimationPath).has_value();

    archive = std::make_unique<Layout::LayoutArchive>(std::move(files));
    layout = std::make_unique<Layout::LiveLayout>(device, *archive, archive->ReadLayout(std::string(IconLayoutName)));
    ShowOnlyEnglish();
    if (!hasAnimation)
        return;

    std::vector<Formats::Pane*> panes = {&layout->RootPane()};
    animation = std::make_unique<MenuAnimation>(*layout, archive->ReadAnimation(std::string(IconLayoutName)), Layout::PlaybackMode::Loop, panes);
    animation->Start();
}

void ChannelIconLayout::ShowOnlyEnglish()
{
    for (std::string_view group : OtherLanguageGroups)
        SetGroupVisible(*layout, group, false);

    SetGroupVisible(*layout, EnglishGroup, true);
}

void ChannelIconLayout::Place(Formats::Vector2 point, Formats::Vector2 scale, Formats::Vector2 boxSize)
{
    layout->RootPane().translation = {point.x, point.y, 0};
    layout->RootPane().scale = scale;

    float halfWidth = boxSize.x * scale.x / 2;
    float halfHeight = boxSize.y * scale.y / 2;
    box = {point.x - halfWidth, point.x + halfWidth, point.y - halfHeight, point.y + halfHeight};
}

void ChannelIconLayout::Calculate()
{
    if (animation)
        animation->Step();
}

void ChannelIconLayout::Draw()
{
    device.SetScissor(box);
    Layout::DrawLayout(device, *layout);
    device.ClearScissor();
}

SaveIconPictures::SaveIconPictures(const Layout::LayoutArchive& layoutArchive, std::optional<Formats::RgbaImage> saveIcon)
    : archive(layoutArchive)
    , icon(std::move(saveIcon))
{
}

std::optional<Formats::RgbaImage> SaveIconPictures::FindPicture(const std::string& textureFileName) const
{
    if (textureFileName == EmptyPictureName && icon)
        return icon;

    return archive.FindPicture(textureFileName);
}

}
