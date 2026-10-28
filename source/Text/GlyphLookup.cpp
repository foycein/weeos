#include "Text/GlyphLookup.h"

namespace Text
{

namespace
{

constexpr std::size_t CharacterCodeCount = 0x10000;
constexpr std::uint16_t NoGlyph = 0xFFFF;

// A character that no map has looked at yet.
constexpr std::uint16_t NotDecided = 0xFFFE;

using GlyphTable = std::vector<std::uint16_t>;

// The first map whose range holds a character decides which glyph it has, even if that map has no glyph for it.
// A later map cannot change that.
void Decide(GlyphTable& glyphTable, std::uint32_t characterCode, std::uint16_t glyphIndex)
{
    if (glyphTable[characterCode] == NotDecided)
        glyphTable[characterCode] = glyphIndex;
}

void AddDirectMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    for (std::uint32_t characterCode = map.firstCharacterCode; characterCode <= map.lastCharacterCode; ++characterCode)
    {
        std::uint32_t glyphIndex = map.firstGlyphIndex + (characterCode - map.firstCharacterCode);
        Decide(glyphTable, characterCode, static_cast<std::uint16_t>(glyphIndex));
    }
}

void AddTableMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    for (std::uint32_t characterCode = map.firstCharacterCode; characterCode <= map.lastCharacterCode; ++characterCode)
        Decide(glyphTable, characterCode, map.glyphIndices.at(characterCode - map.firstCharacterCode));
}

void AddScanMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    for (const Formats::CharacterMapPair& pair : map.pairs)
    {
        bool isInRange = pair.characterCode >= map.firstCharacterCode && pair.characterCode <= map.lastCharacterCode;
        if (isInRange)
            Decide(glyphTable, pair.characterCode, pair.glyphIndex);
    }
}

void AddMap(GlyphTable& glyphTable, const Formats::CharacterMap& map)
{
    switch (map.method)
    {
    case Formats::CharacterMapMethod::Direct:
        AddDirectMap(glyphTable, map);
        break;

    case Formats::CharacterMapMethod::Table:
        AddTableMap(glyphTable, map);
        break;

    case Formats::CharacterMapMethod::Scan:
        AddScanMap(glyphTable, map);
        break;
    }

    // The characters of the range that the map does not list have no glyph, and no later map can give them one.
    for (std::uint32_t characterCode = map.firstCharacterCode; characterCode <= map.lastCharacterCode; ++characterCode)
        Decide(glyphTable, characterCode, NoGlyph);
}

}

GlyphLookup::GlyphLookup(const Formats::Font& font)
    : glyphIndices(CharacterCodeCount, NotDecided)
    , replacementGlyphIndex(font.defaultGlyphIndex)
{
    for (const Formats::CharacterMap& map : font.characterMaps)
        AddMap(glyphIndices, map);
}

bool GlyphLookup::HasGlyph(char16_t character) const
{
    std::uint16_t glyphIndex = glyphIndices[character];
    return glyphIndex != NoGlyph && glyphIndex != NotDecided;
}

std::uint16_t GlyphLookup::FindGlyphIndex(char16_t character) const
{
    if (HasGlyph(character))
        return glyphIndices[character];

    return replacementGlyphIndex;
}

}
