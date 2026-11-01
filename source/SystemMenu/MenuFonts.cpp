#include "SystemMenu/MenuFonts.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/U8Archive.h"

#include <format>
#include <optional>
#include <string>
#include <utility>

namespace SystemMenu
{

namespace
{

// The two small fonts are files of an archive that is a shared content of the title.
constexpr std::uint16_t SmallFontsContentIndex = 5;
constexpr std::string_view FirstSmallFontFile = "wbf1.brfna";
constexpr std::string_view SecondSmallFontFile = "wbf2.brfna";

// The large font is in a packed archive of the resource archive.
constexpr std::string_view LargeFontArchivePath = "font/font.ash";
constexpr std::string_view LargeFontFile = "arc/font/RevoIpl_RodinNTLGPro_DB_48_IA4.brfnt";

Formats::Font ReadFontFile(const Formats::U8Archive& archive, std::string_view fileName)
{
    std::optional<Formats::ByteView> file = archive.FindFile(fileName);
    if (!file)
        throw MenuError(std::format("The font archive has no file {}", fileName));

    return Formats::ReadFont(*file);
}

}

MenuFonts::LoadedFont::LoadedFont(Graphics::GraphicsDevice& device, Formats::Font loadedFont)
    : font(std::move(loadedFont))
    , layouter(font)
    , drawer(device, font)
{
}

MenuFonts::MenuFonts(Graphics::GraphicsDevice& device, TitleContents& titleContents, const MenuFiles& files)
{
    Formats::U8Archive smallFonts = Formats::ReadU8Archive(titleContents.ReadContent(SmallFontsContentIndex));
    AddFont(device, ReadFontFile(smallFonts, FirstSmallFontFile),
        {"RevoIpl_RodinNTLGPro_DB_32_I4.brfnt", "WiiBitmapFontType1.brfnt", FirstSmallFontFile});
    AddFont(device, ReadFontFile(smallFonts, SecondSmallFontFile),
        {"RevoIpl_UtrilloProGrecoStd_M_32_I4.brfnt", "WiiBitmapFontType2.brfnt", SecondSmallFontFile});

    Formats::U8Archive largeFontArchive = Formats::ReadU8Archive(Formats::DecompressAsh0(files.ReadFile(LargeFontArchivePath)));
    AddFont(device, ReadFontFile(largeFontArchive, LargeFontFile), {"RevoIpl_RodinNTLGPro_DB_48_IA4.brfnt"});
}

void MenuFonts::AddFont(Graphics::GraphicsDevice& device, Formats::Font font, std::initializer_list<std::string_view> layoutNames)
{
    loadedFonts.push_back(std::make_unique<LoadedFont>(device, std::move(font)));
    LoadedFont& loadedFont = *loadedFonts.back();

    for (std::string_view layoutName : layoutNames)
        textFonts.Add(std::string(layoutName), loadedFont.layouter, loadedFont.drawer);
}

const Layout::TextFonts& MenuFonts::Fonts() const
{
    return textFonts;
}

}
