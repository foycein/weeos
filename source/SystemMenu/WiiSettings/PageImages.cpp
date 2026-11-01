#include "SystemMenu/WiiSettings/PageImages.h"

#include "SystemMenu/MenuError.h"

#include "Formats/GifImage.h"
#include "Formats/Lz77.h"
#include "Formats/PngImage.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view PagesArchivePath = "html/US2/iplsetting.ash";
constexpr std::string_view PngExtension = ".png";

}

PageImages::PageImages(Graphics::GraphicsDevice& graphicsDevice, const MenuFiles& files)
    : device(graphicsDevice)
    , pages(Formats::ReadU8Archive(files.ReadFile(PagesArchivePath)))
{
}

PageImages::~PageImages()
{
    for (const auto& [path, image] : images)
        device.DestroyTexture(image.texture);
}

const PageImage& PageImages::Find(const std::string& path)
{
    auto found = images.find(path);
    if (found != images.end())
        return found->second;

    return images.emplace(path, Make(path)).first->second;
}

PageImage PageImages::Make(const std::string& path) const
{
    std::optional<Formats::ByteView> packedFile = pages.FindFile(path);
    if (!packedFile)
        throw MenuError(std::format("The pages of Wii Settings have no picture {}", path));

    Formats::Bytes file = Formats::DecompressLz77(*packedFile);
    Formats::RgbaImage image = path.ends_with(PngExtension) ? Formats::ReadPngImage(file) : Formats::ReadGifImage(file);

    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;

    PageImage pageImage;
    pageImage.texture = device.CreateTexture(description);
    pageImage.width = static_cast<int>(image.width);
    pageImage.height = static_cast<int>(image.height);
    return pageImage;
}

}
