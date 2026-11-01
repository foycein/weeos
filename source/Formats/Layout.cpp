#include "Formats/Layout.h"

#include "Formats/BitFields.h"
#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/SectionedFile.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "Layout";
// The System Menu and the channels write version 8. The Internet Channel's cursors are version 10 with the same sections (LAY-08).
constexpr std::uint16_t SupportedVersion = 8;
constexpr std::uint16_t LaterSupportedVersion = 10;
constexpr int MaximumNestingDepth = 64;

constexpr std::size_t PaneNameSize = 16;
constexpr std::size_t UserInformationSize = 8;
constexpr std::size_t GroupNameSize = 16;
constexpr std::size_t NameListEntryUnusedSize = 4;

constexpr int VisibleBit = 0;
constexpr int InheritsAlphaBit = 1;
constexpr int AdjustsLocationBit = 2;

// The origin of a pane is one number from 0 to 8 that counts the nine positions row by row.
constexpr unsigned OriginColumnCount = 3;
constexpr unsigned HighestOriginNumber = 8;

constexpr std::size_t UnusedAfterPaneAlphaSize = 1;
constexpr std::size_t UnusedAfterCenteredSize = 3;
constexpr std::size_t UnusedAfterNameCountSize = 2;
constexpr std::size_t UnusedAfterContentSize = 1;
constexpr std::size_t UnusedAfterTextAlignmentSize = 2;
constexpr std::size_t UnusedAfterWindowFrameCountSize = 3;
constexpr std::size_t UnusedAfterWindowFlipSize = 1;

void ReadLayoutProperties(const Section& section, Layout& layout)
{
    ByteReader reader = OpenSection(section, DataDescription);
    layout.isCentered = reader.ReadByte() != 0;
    reader.Skip(UnusedAfterCenteredSize);
    layout.width = reader.ReadFloat();
    layout.height = reader.ReadFloat();
}

// The names are stored behind a table. Each table entry gives the offset of a name, counted from the start of the table.
std::vector<std::string> ReadNameList(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);
    std::uint16_t nameCount = reader.ReadUint16();
    reader.Skip(UnusedAfterNameCountSize);
    std::size_t tableStart = reader.Position();

    std::vector<std::string> names;
    for (std::uint16_t nameNumber = 0; nameNumber < nameCount; ++nameNumber)
    {
        std::uint32_t nameOffset = reader.ReadUint32();
        reader.Skip(NameListEntryUnusedSize);

        ByteReader nameReader = OpenSection(section, DataDescription);
        nameReader.MoveTo(tableStart + nameOffset);
        names.push_back(nameReader.ReadTextUntilZero());
    }
    return names;
}

void RequireSectionEnd(const ByteReader& reader)
{
    if (!reader.IsAtEnd())
        reader.Fail(std::format("{} bytes of the section are not understood", reader.RemainingSize()));
}

Pane ReadPaneBase(ByteReader& reader, PaneKind kind)
{
    Pane pane;
    pane.kind = kind;

    Byte flags = reader.ReadByte();
    pane.isVisible = ExtractBits(flags, VisibleBit, 1) != 0;
    pane.inheritsAlpha = ExtractBits(flags, InheritsAlphaBit, 1) != 0;
    pane.adjustsLocationForWideScreen = ExtractBits(flags, AdjustsLocationBit, 1) != 0;

    unsigned originNumber = reader.ReadByte();
    if (originNumber > HighestOriginNumber)
        reader.Fail(std::format("the origin {} is not one of the nine positions", originNumber));
    pane.horizontalOrigin = static_cast<HorizontalOrigin>(originNumber % OriginColumnCount);
    pane.verticalOrigin = static_cast<VerticalOrigin>(originNumber / OriginColumnCount);

    pane.alpha = reader.ReadByte();
    reader.Skip(UnusedAfterPaneAlphaSize);
    pane.name = reader.ReadText(PaneNameSize);
    pane.userInformation = reader.ReadText(UserInformationSize);
    pane.translation = ReadVector3(reader);
    pane.rotation = ReadVector3(reader);
    pane.scale = ReadVector2(reader);
    pane.width = reader.ReadFloat();
    pane.height = reader.ReadFloat();
    return pane;
}

TextureCoordinateSet ReadTextureCoordinateSet(ByteReader& reader)
{
    TextureCoordinateSet set;
    for (Vector2& corner : set.corners)
        corner = ReadVector2(reader);
    return set;
}

PaneContent ReadPaneContent(ByteReader& reader)
{
    PaneContent content;
    for (Rgba8Color& color : content.vertexColors)
        color = ReadRgba8Color(reader);

    content.materialIndex = reader.ReadUint16();
    Byte setCount = reader.ReadByte();
    reader.Skip(UnusedAfterContentSize);

    for (Byte setNumber = 0; setNumber < setCount; ++setNumber)
        content.textureCoordinateSets.push_back(ReadTextureCoordinateSet(reader));

    return content;
}

Pane ReadPlainPane(const Section& section, PaneKind kind)
{
    ByteReader reader = OpenSection(section, DataDescription);
    Pane pane = ReadPaneBase(reader, kind);
    RequireSectionEnd(reader);
    return pane;
}

Pane ReadPicturePane(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);
    Pane pane = ReadPaneBase(reader, PaneKind::Picture);
    pane.picture = ReadPaneContent(reader);
    RequireSectionEnd(reader);
    return pane;
}

std::u16string ReadText(const Section& section, std::uint32_t textOffset, std::uint16_t textSize)
{
    ByteReader reader(section.bytes, DataDescription);
    reader.MoveTo(textOffset);

    std::u16string text;
    std::size_t characterCount = textSize / sizeof(char16_t);
    for (std::size_t characterNumber = 0; characterNumber < characterCount; ++characterNumber)
    {
        char16_t character = reader.ReadUint16();
        if (character == 0)
            break;

        text.push_back(character);
    }
    return text;
}

Pane ReadTextBoxPane(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);
    Pane pane = ReadPaneBase(reader, PaneKind::TextBox);

    TextBox textBox;
    textBox.maximumTextSize = reader.ReadUint16();
    std::uint16_t textSize = reader.ReadUint16();
    textBox.materialIndex = reader.ReadUint16();
    textBox.fontIndex = reader.ReadUint16();
    textBox.textOrigin = reader.ReadByte();
    textBox.lineAlignment = reader.ReadByte();
    reader.Skip(UnusedAfterTextAlignmentSize);
    std::uint32_t textOffset = reader.ReadUint32();
    textBox.topColor = ReadRgba8Color(reader);
    textBox.bottomColor = ReadRgba8Color(reader);
    textBox.fontSize = ReadVector2(reader);
    textBox.characterSpace = reader.ReadFloat();
    textBox.lineSpace = reader.ReadFloat();
    textBox.text = ReadText(section, textOffset, textSize);

    pane.textBox = textBox;
    return pane;
}

WindowFrame ReadWindowFrame(ByteReader& reader)
{
    WindowFrame frame;
    frame.materialIndex = reader.ReadUint16();
    frame.textureFlip = reader.ReadByte();
    reader.Skip(UnusedAfterWindowFlipSize);
    return frame;
}

// The window has a table with the offsets of its frames, and its content lies at another offset.
Pane ReadWindowPane(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);
    Pane pane = ReadPaneBase(reader, PaneKind::Window);

    Window window;
    window.magnificationLeft = reader.ReadFloat();
    window.magnificationRight = reader.ReadFloat();
    window.magnificationUp = reader.ReadFloat();
    window.magnificationDown = reader.ReadFloat();
    Byte frameCount = reader.ReadByte();
    reader.Skip(UnusedAfterWindowFrameCountSize);
    std::uint32_t contentOffset = reader.ReadUint32();
    std::uint32_t frameTableOffset = reader.ReadUint32();

    reader.MoveTo(contentOffset);
    window.content = ReadPaneContent(reader);

    reader.MoveTo(frameTableOffset);
    std::vector<std::uint32_t> frameOffsets;
    for (Byte frameNumber = 0; frameNumber < frameCount; ++frameNumber)
        frameOffsets.push_back(reader.ReadUint32());

    for (std::uint32_t frameOffset : frameOffsets)
    {
        reader.MoveTo(frameOffset);
        window.frames.push_back(ReadWindowFrame(reader));
    }

    pane.window = window;
    return pane;
}

Pane ReadPaneSection(const Section& section)
{
    if (section.tag == "pan1")
        return ReadPlainPane(section, PaneKind::Null);

    if (section.tag == "bnd1")
        return ReadPlainPane(section, PaneKind::Boundary);

    if (section.tag == "pic1")
        return ReadPicturePane(section);

    if (section.tag == "txt1")
        return ReadTextBoxPane(section);

    if (section.tag == "wnd1")
        return ReadWindowPane(section);

    throw FormatError(std::format("{}: the section {} is not a pane", DataDescription, section.tag));
}

// A pane may be followed by a "pas1" section, then its child panes, and a "pae1" section that ends the children.
Pane ReadPaneAndChildren(SectionCursor& cursor, int depth)
{
    if (depth > MaximumNestingDepth)
        throw FormatError(std::format("{}: the panes are nested more than {} levels deep", DataDescription, MaximumNestingDepth));

    Pane pane = ReadPaneSection(cursor.Take());

    if (cursor.NextIs("pas1"))
    {
        cursor.Take();
        while (!cursor.NextIs("pae1"))
            pane.children.push_back(ReadPaneAndChildren(cursor, depth + 1));
        cursor.Take();
    }

    return pane;
}

Group ReadGroupSection(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);

    Group group;
    group.name = reader.ReadText(GroupNameSize);
    std::uint16_t paneCount = reader.ReadUint16();
    reader.Skip(UnusedAfterNameCountSize);

    for (std::uint16_t paneNumber = 0; paneNumber < paneCount; ++paneNumber)
        group.paneNames.push_back(reader.ReadText(PaneNameSize));

    RequireSectionEnd(reader);
    return group;
}

// A group may be followed by a "grs1" section, then its child groups, and a "gre1" section that ends the children.
Group ReadGroupAndChildren(SectionCursor& cursor, int depth)
{
    if (depth > MaximumNestingDepth)
        throw FormatError(std::format("{}: the groups are nested more than {} levels deep", DataDescription, MaximumNestingDepth));

    Group group = ReadGroupSection(cursor.Take("grp1"));

    if (cursor.NextIs("grs1"))
    {
        cursor.Take();
        while (!cursor.NextIs("gre1"))
            group.children.push_back(ReadGroupAndChildren(cursor, depth + 1));
        cursor.Take();
    }

    return group;
}

}

Layout ReadLayout(ByteView bytes)
{
    SectionedFile file = ReadSectionedFile(bytes, "RLYT", DataDescription);
    if (file.version != SupportedVersion && file.version != LaterSupportedVersion)
    {
        throw FormatError(std::format("{}: the version {} is not supported, only versions {} and {} are known", DataDescription,
            file.version, SupportedVersion, LaterSupportedVersion));
    }

    SectionCursor cursor(file.sections, DataDescription);

    Layout layout;
    ReadLayoutProperties(cursor.Take("lyt1"), layout);

    if (cursor.NextIs("txl1"))
        layout.textureNames = ReadNameList(cursor.Take());

    if (cursor.NextIs("fnl1"))
        layout.fontNames = ReadNameList(cursor.Take());

    layout.materials = ReadMaterials(cursor.Take("mat1"));
    layout.rootPane = ReadPaneAndChildren(cursor, 0);

    if (cursor.NextIs("grp1"))
        layout.rootGroup = ReadGroupAndChildren(cursor, 0);

    if (!cursor.IsAtEnd())
        throw FormatError(std::format("{}: the section {} comes at a place where no section is expected", DataDescription, cursor.Take().tag));

    return layout;
}

}
