#include "Text/GlyphLookup.h"

namespace Text
{

namespace
{

constexpr std::size_t CharacterCodeCount = 0x10000;
constexpr std::uint16_t NoGlyph = 0xFFFF;

using GlyphTable = std::vector<std::uint16_t>;

// The first map that has a glyph for a character decides. A later map cannot change it.
void SetGlyph(GlyphTable& glyphTable, std::uint32_t characterCode, std::uint16_t glyphIndex)
{
    if (glyphIndex == NoGlyph)
        return;

    if (glyphTable[characterCode] != NoGlyph)
        return;

    glyphTable[characterCode] = glyphIndex;
}

void AddDirectMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    for (std::uint32_t characterCode = map.firstCharacterCode; characterCode <= map.lastCharacterCode; ++characterCode)
    {
        std::uint32_t glyphIndex = map.firstGlyphIndex + (characterCode - map.firstCharacterCode);
        SetGlyph(glyphTable, characterCode, static_cast<std::uint16_t>(glyphIndex));
    }
}

void AddTableMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    for (std::uint32_t characterCode = map.firstCharacterCode; characterCode <= map.lastCharacterCode; ++characterCode)
        SetGlyph(glyphTable, characterCode, map.glyphIndices.at(characterCode - map.firstCharacterCode));
}

void AddScanMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    for (const Formats::CharacterMapPair& pair : map.pairs)
    {
        bool isInRange = pair.characterCode >= map.firstCharacterCode && pair.characterCode <= map.lastCharacterCode;
        if (isInRange)
            SetGlyph(glyphTable, pair.characterCode, pair.glyphIndex);
    }
}

void AddMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    switch (map.method)
    {
    case Formats::CharacterMapMethod::Direct:
        AddDirectMap(glyphTable, map);
        return;

    case Formats::CharacterMapMethod::Table:
        AddTableMap(glyphTable, map);
        return;

    case Formats::CharacterMapMethod::Scan:
        AddScanMap(glyphTable, map);
        return;
    }
}

}

GlyphLookup::GlyphLookup(const Formats::Font& font)
    : glyphIndices(CharacterCodeCount, NoGlyph)
    , replacementGlyphIndex(font.defaultGlyphIndex)
{
    for (const Formats::CharacterMap& map : font.characterMaps)
        AddMap(glyphIndices, map);
}

bool GlyphLookup::HasGlyph(char16_t character) const
{
    return glyphIndices[character] != NoGlyph;
}

std::uint16_t GlyphLookup::FindGlyphIndex(char16_t character) const
{
    if (HasGlyph(character))
        return glyphIndices[character];

    return replacementGlyphIndex;
}

}
