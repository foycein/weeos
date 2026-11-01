#pragma once

#include "SystemMenu/MenuAnimation.h"

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/LayoutArchive.h"
#include "Layout/LiveLayout.h"

#include <memory>
#include <optional>

namespace SystemMenu
{

// The icon of a channel: a small layout of the channel's own, the file meta/icon.bin of its banner. The slots of Data Management and
// the tiles of the SD Card Menu show it, smaller, and play its animation "icon" over and over.
class ChannelIconLayout
{
public:
    // Throws a FormatError or a LayoutError when the file is not an icon. The device must outlive the icon.
    ChannelIconLayout(Graphics::GraphicsDevice& device, Formats::ByteView iconFile);

    // Puts the middle of the icon on the point of the picture (in layout units) and scales it. Nothing of it is drawn outside the
    // box of this size around the point, before the scale.
    void Place(Formats::Vector2 point, Formats::Vector2 scale, Formats::Vector2 boxSize);

    // One step of the animation.
    void Calculate();

    void Draw();

private:
    void ShowOnlyEnglish();

    Graphics::GraphicsDevice& device;
    Graphics::ScissorRectangle box;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<Layout::LiveLayout> layout;
    std::unique_ptr<MenuAnimation> animation;
};

// The pictures of a slot of save data: the icon of the save data takes the place of the empty picture of the layout.
class SaveIconPictures : public Layout::PictureSource
{
public:
    // The archive must outlive this object.
    SaveIconPictures(const Layout::LayoutArchive& archive, std::optional<Formats::RgbaImage> icon);

    std::optional<Formats::RgbaImage> FindPicture(const std::string& textureFileName) const override;

private:
    const Layout::LayoutArchive& archive;
    std::optional<Formats::RgbaImage> icon;
};

}
