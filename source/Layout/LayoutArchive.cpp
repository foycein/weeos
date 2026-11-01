#include "Layout/LayoutArchive.h"

#include "Layout/LayoutError.h"

#include "Formats/TplTexture.h"

#include <format>
#include <vector>

namespace Layout
{

namespace
{

const std::string StandardRootFolder = "arc/";
const std::string LayoutsFolder = "blyt/";
const std::string AnimationsFolder = "anim/";
const std::string PicturesFolder = "timg/";

const std::string LayoutExtension = ".brlyt";
const std::string AnimationExtension = ".brlan";

}

LayoutArchive::LayoutArchive(Formats::U8Archive archiveToUse)
    : LayoutArchive(std::move(archiveToUse), StandardRootFolder)
{
}

LayoutArchive::LayoutArchive(Formats::U8Archive archiveToUse, std::string rootFolder)
    : archive(std::move(archiveToUse))
    , root(std::move(rootFolder))
{
}

Formats::ByteView LayoutArchive::FindFileOrFail(const std::string& path) const
{
    std::optional<Formats::ByteView> file = archive.FindFile(path);
    if (!file)
        throw LayoutError(std::format("The layout archive has no file {}", path));

    return *file;
}

Formats::Layout LayoutArchive::ReadLayout(const std::string& layoutName) const
{
    return Formats::ReadLayout(FindFileOrFail(root + LayoutsFolder + layoutName + LayoutExtension));
}

Formats::LayoutAnimation LayoutArchive::ReadAnimation(const std::string& animationName) const
{
    return Formats::ReadLayoutAnimation(FindFileOrFail(root + AnimationsFolder + animationName + AnimationExtension));
}

std::optional<Formats::RgbaImage> LayoutArchive::FindPicture(const std::string& textureFileName) const
{
    std::optional<Formats::ByteView> file = archive.FindFile(root + PicturesFolder + textureFileName);
    if (!file)
        return std::nullopt;

    std::vector<Formats::TplTexture> textures = Formats::ReadTplTextures(*file);
    if (textures.empty())
        throw LayoutError(std::format("The texture file {} holds no image", textureFileName));

    return Formats::DecodeTexture(textures.front());
}

}
