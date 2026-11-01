#pragma once

#include "Layout/PictureSource.h"

#include "Formats/Layout.h"
#include "Formats/LayoutAnimation.h"
#include "Formats/U8Archive.h"

#include <optional>
#include <string>

namespace Layout
{

// The files of a layout bundle: the U8 archive inside an .ash file of the System Menu, or inside the banner or the icon of a channel.
// The archive holds the folders "arc/blyt" with the layouts, "arc/anim" with the animations and "arc/timg" with the pictures.
//
// It gives the pictures to the layouts that are built from it.
class LayoutArchive : public PictureSource
{
public:
    explicit LayoutArchive(Formats::U8Archive archive);

    // An archive whose folders blyt, anim and timg lie in another folder than "arc/", or at its root (rootFolder ""), as in the
    // Layout00.szs of the Mii Channel and the layout bundles of the Wii Speak Channel.
    LayoutArchive(Formats::U8Archive archive, std::string rootFolder);

    // Reads "arc/blyt/<layoutName>.brlyt", for example the layoutName "my_IplTop_c".
    // Throws a LayoutError when the archive has no such file and a FormatError when the file is damaged.
    Formats::Layout ReadLayout(const std::string& layoutName) const;

    // Reads "arc/anim/<animationName>.brlan", for example the animationName "it_Has_a_SeenIn".
    Formats::LayoutAnimation ReadAnimation(const std::string& animationName) const;

    // Reads and decodes the first image of "arc/timg/<textureFileName>", for example "my_Balloon_a.tpl".
    std::optional<Formats::RgbaImage> FindPicture(const std::string& textureFileName) const override;

private:
    Formats::ByteView FindFileOrFail(const std::string& path) const;

    Formats::U8Archive archive;
    std::string root;
};

}
