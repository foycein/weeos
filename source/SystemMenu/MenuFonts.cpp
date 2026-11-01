#include "SystemMenu/MenuFonts.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/U8Archive.h"

#include <format>
#include <optional>

namespace SystemMenu
{

namespace
{

// The content that holds the two bitmap fonts.
constexpr std::uint16_t FontContentIndex = 5;

constexpr std::string_view FirstBitmapFontFile = "wbf1.brfna";
constexpr std::string_view SecondBitmapFontFile = "wbf2.brfna";
constexpr std::string_view LargeFontArchivePath = "font/font.ash";
constexpr std::string_view LargeFontPath = "arc/font/RevoIpl_RodinNTLGPro_DB_48_IA4.brfnt";

// The names that the layouts of the menu give to their three fonts.
const std::vector<std::string> FirstBitmapFontNames = {"RevoIpl_RodinNTLGPro_DB_32_I4.brfnt", "WiiBitmapFontType1.brfnt", "wbf1.brfna"};
const std::vector<std::string> SecondBitmapFontNames = {"RevoIpl_UtrilloProGrecoStd_M_32_I4.brfnt", "WiiBitmapFontType2.brfnt", "wbf2.brfna"};
const std::vector<std::string> LargeFontNames = {"RevoIpl_RodinNTLGPro_DB_48_IA4.brfnt"};

Formats::Font ReadFontFile(const Formats::U8Archive& archive, std::string_view path)
{
    std::optional<Formats::ByteView> file = archive.FindFile(path);
    if (!file)
        throw MenuError(std::format("The font archive has no file {}", path));

    return Formats::ReadFont(*file);
}

}

MenuFonts::MenuFonts(Graphics::GraphicsDevice& graphicsDevice, TitleContents& titleContents, const MenuFiles& menuFiles)
    : device(graphicsDevice)
{
    Formats::U8Archive bitmapFonts = Formats::ReadU8Archive(titleContents.ReadContent(FontContentIndex));
    AddFont(ReadFontFile(bitmapFonts, FirstBitmapFontFile), FirstBitmapFontNames);
    AddFont(ReadFontFile(bitmapFonts, SecondBitmapFontFile), SecondBitmapFontNames);

    Formats::U8Archive largeFontArchive = Formats::ReadU8Archive(Formats::DecompressAsh0(menuFiles.ReadFile(LargeFontArchivePath)));
    AddFont(ReadFontFile(largeFontArchive, LargeFontPath), LargeFontNames);
}

void MenuFonts::AddFont(Formats::Font font, const std::vector<std::string>& names)
{
    fonts.push_back(std::make_unique<Formats::Font>(std::move(font)));
    layouters.push_back(std::make_unique<Text::TextLayouter>(*fonts.back()));
    drawers.push_back(std::make_unique<Text::TextDrawer>(device, *fonts.back()));

    for (const std::string& name : names)
        textFonts.Add(name, *layouters.back(), *drawers.back());
}

const Layout::TextFonts& MenuFonts::Fonts() const
{
    return textFonts;
}

}
