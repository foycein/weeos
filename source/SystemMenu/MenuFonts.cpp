#include "SystemMenu/MenuFonts.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/U8Archive.h"

#include <format>
#include <utility>

namespace SystemMenu
{

namespace
{

constexpr std::string_view FirstSystemFontFile = "wbf1.brfna";
constexpr std::string_view SecondSystemFontFile = "wbf2.brfna";
constexpr std::string_view LargeFontArchivePath = "font/font.ash";
constexpr std::string_view LargeFontFilePath = "arc/font/RevoIpl_RodinNTLGPro_DB_48_IA4.brfnt";

Formats::Font ReadFontOf(const Formats::U8Archive& archive, std::string_view path)
{
    std::optional<Formats::ByteView> file = archive.FindFile(path);
    if (!file)
        throw MenuError(std::format("The archive of the fonts has no file {}", path));

    return Formats::ReadFont(*file);
}

}

MenuFonts::DrawableFontSet::DrawableFontSet(Graphics::GraphicsDevice& device, Formats::Font fontToDraw)
    : font(std::move(fontToDraw))
    , layouter(font)
    , drawer(device, font)
{
}

MenuFonts::MenuFonts(Graphics::GraphicsDevice& device, TitleContents& titleContents, const MenuFiles& files)
{
    Formats::U8Archive systemFonts = Formats::ReadU8Archive(titleContents.ReadContent(FontContentIndex));
    Formats::U8Archive largeFonts = Formats::ReadU8Archive(Formats::DecompressAsh0(files.ReadFile(LargeFontArchivePath)));

    // The layouts name each font by the file it was made as, and some by the name of an older file. The names mean the same font.
    AddFont(device, ReadFontOf(systemFonts, FirstSystemFontFile),
        {"RevoIpl_RodinNTLGPro_DB_32_I4.brfnt", "WiiBitmapFontType1.brfnt", FirstSystemFontFile});
    AddFont(device, ReadFontOf(systemFonts, SecondSystemFontFile),
        {"RevoIpl_UtrilloProGrecoStd_M_32_I4.brfnt", "WiiBitmapFontType2.brfnt", SecondSystemFontFile});
    AddFont(device, ReadFontOf(largeFonts, LargeFontFilePath), {"RevoIpl_RodinNTLGPro_DB_48_IA4.brfnt"});
}

const Layout::TextFonts& MenuFonts::Fonts() const
{
    return fonts;
}

void MenuFonts::AddFont(Graphics::GraphicsDevice& device, Formats::Font font, std::initializer_list<std::string_view> layoutNames)
{
    fontSets.push_back(std::make_unique<DrawableFontSet>(device, std::move(font)));
    DrawableFontSet& fontSet = *fontSets.back();

    for (std::string_view layoutName : layoutNames)
        fonts.Add(std::string(layoutName), fontSet.layouter, fontSet.drawer);
}

}
