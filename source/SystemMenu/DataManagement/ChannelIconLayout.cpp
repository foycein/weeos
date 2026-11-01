#include "SystemMenu/DataManagement/ChannelIconLayout.h"

#include "SystemMenu/WiiMenu/ChannelTile.h"

#include "Formats/BannerContent.h"
#include "Layout/LayoutDrawer.h"

#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view IconLayoutName = "icon";
constexpr std::string_view IconAnimationPath = "arc/anim/icon.brlan";

// The layouts of the slots show this picture where the icon of the save data goes.
constexpr std::string_view EmptyPictureName = "it_DummyPicture_a.tpl";

}

ChannelIconLayout::ChannelIconLayout(Graphics::GraphicsDevice& graphicsDevice, Formats::ByteView iconFile)
    : device(graphicsDevice)
{
    Formats::U8Archive files = Formats::ReadLayoutFile(iconFile);
    bool hasAnimation = files.FindFile(IconAnimationPath).has_value();

    archive = std::make_unique<Layout::LayoutArchive>(std::move(files));
    Formats::Layout file = archive->ReadLayout(std::string(IconLayoutName));
    layout = std::make_unique<Layout::LiveLayout>(device, *archive, file);
    ShowOnlyTheEnglishPanes(*layout, file);
    if (!hasAnimation)
        return;

    std::vector<Formats::Pane*> panes = {&layout->RootPane()};
    animation = std::make_unique<MenuAnimation>(*layout, archive->ReadAnimation(std::string(IconLayoutName)), Layout::PlaybackMode::Loop, panes);
    animation->Start();
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
