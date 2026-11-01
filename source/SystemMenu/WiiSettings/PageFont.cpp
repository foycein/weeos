#include "SystemMenu/WiiSettings/PageFont.h"

#include "SystemMenu/MenuError.h"

#include "Formats/U8Archive.h"

#include <format>

namespace SystemMenu
{

namespace
{

// The two small fonts of the menu are the files of an archive that is a shared content of the title. The first one has every letter and digit.
constexpr std::uint16_t SmallFontsContentIndex = 5;
constexpr std::string_view FontFilePath = "wbf1.brfna";

Formats::Font ReadMenuFont(const MenuFiles& files)
{
    Formats::U8Archive fontArchive = Formats::ReadU8Archive(files.ReadSharedContent(SmallFontsContentIndex));
    std::optional<Formats::ByteView> fontFile = fontArchive.FindFile(FontFilePath);
    if (!fontFile)
        throw MenuError(std::format("The font archive of the menu has no file {}", FontFilePath));

    return Formats::ReadFont(*fontFile);
}

}

PageFont::PageFont(Graphics::GraphicsDevice& device, const MenuFiles& files)
    : font(ReadMenuFont(files))
    , layouter(font)
    , drawer(device, font)
{
}

void PageFont::Draw(const std::u16string& text, const PageRectangle& bounds, const Text::TextAlignment& alignment,
    const Formats::Rgba8Color& color, float sizeInPoints)
{
    if (text.empty())
        return;

    // The font has its own size when the characters are as large as the font says. A line of the page is its height.
    float scale = sizeInPoints / static_cast<float>(font.height);

    Text::TextStyle style;
    style.fontSize = {static_cast<float>(font.width) * scale, static_cast<float>(font.height) * scale};
    style.topColor = color;
    style.bottomColor = color;
    style.alignment = alignment;

    // The page has its origin at the top left corner and y grows downward. The picture has its origin in the middle and y grows upward.
    Text::TextArea area;
    area.left = static_cast<float>(bounds.left - PageWidth / 2);
    area.top = static_cast<float>(PageHeight / 2 - bounds.top);
    area.width = static_cast<float>(bounds.width);
    area.height = static_cast<float>(bounds.height);

    drawer.Draw(layouter.LayOut(text, style, area));
}

}
