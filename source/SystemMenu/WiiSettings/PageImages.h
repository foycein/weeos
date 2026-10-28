#pragma once

#include "SystemMenu/MenuFiles.h"

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"

#include <map>
#include <string>

namespace SystemMenu
{

// A picture of the pages that lives on the graphics device.
struct PageImage
{
    Graphics::TextureHandle texture;
    int width = 0;
    int height = 0;
};

// The pictures of the Wii Settings pages: the buttons, the arrows, the tabs and the backgrounds. They are the GIF and PNG files of the
// archive html/US2/iplsetting.ash in the resources of the menu, packed with LZ77. A picture is made into a texture the first time it is drawn.
class PageImages
{
public:
    // The device and the files must outlive the pictures. Throws a MenuError when the archive of the pages is not in the files.
    PageImages(Graphics::GraphicsDevice& graphicsDevice, const MenuFiles& files);
    ~PageImages();

    PageImages(const PageImages&) = delete;
    PageImages& operator=(const PageImages&) = delete;

    // The path is the one inside the archive, for example "FIX/COMMON/BTN/Under.gif". Throws a MenuError when the archive has no such file.
    const PageImage& Find(const std::string& path);

private:
    PageImage Make(const std::string& path) const;

    Graphics::GraphicsDevice& device;
    Formats::U8Archive pages;
    std::map<std::string, PageImage> images;
};

}
