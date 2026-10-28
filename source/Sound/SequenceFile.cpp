#include "Sound/SequenceFile.h"

#include "Sound/SoundFileHeader.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::string_view DataDescription = "Sequence file";
constexpr std::uint16_t SupportedVersion = 0x0100;
constexpr std::size_t HeaderSize = 0x20;
constexpr std::size_t SectionStartSize = 8;

Formats::Bytes ReadCommands(Formats::ByteView dataSection)
{
    Formats::ByteReader reader(dataSection, DataDescription);
    reader.Skip(SectionStartSize);

    // The data section says where in the section the commands begin.
    std::uint32_t commandsOffset = reader.ReadUint32();
    if (commandsOffset < reader.Position())
        reader.Fail(std::format("the commands begin at {:#x}, inside the header of the section", commandsOffset));

    reader.MoveTo(commandsOffset);
    Formats::ByteView commands = reader.ReadBytes(reader.RemainingSize());
    return Formats::Bytes(commands.begin(), commands.end());
}

SequenceLabel ReadLabel(Formats::ByteReader& reader)
{
    SequenceLabel label;
    label.offset = reader.ReadUint32();
    std::uint32_t nameLength = reader.ReadUint32();
    label.name = reader.ReadText(nameLength);
    return label;
}

std::vector<SequenceLabel> ReadLabels(Formats::ByteView labelSection)
{
    Formats::ByteReader reader(labelSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::uint32_t labelCount = reader.ReadUint32();

    std::vector<std::uint32_t> labelOffsets;
    for (std::uint32_t labelNumber = 0; labelNumber < labelCount; ++labelNumber)
        labelOffsets.push_back(reader.ReadUint32());

    std::vector<SequenceLabel> labels;
    for (std::uint32_t labelOffset : labelOffsets)
    {
        reader.MoveTo(SectionStartSize + labelOffset);
        labels.push_back(ReadLabel(reader));
    }
    return labels;
}

}

std::optional<std::uint32_t> SequenceFile::FindLabelOffset(std::string_view name) const
{
    for (const SequenceLabel& label : labels)
    {
        if (label.name == name)
            return label.offset;
    }

    return std::nullopt;
}

SequenceFile ReadSequenceFile(Formats::ByteView bytes)
{
    SoundFile file = ReadSoundFileHeader(bytes, "RSEQ", SupportedVersion, HeaderSize, {"DATA", "LABL"}, DataDescription);

    SequenceFile sequenceFile;
    sequenceFile.commands = ReadCommands(file.sections[0]);
    sequenceFile.labels = ReadLabels(file.sections[1]);
    return sequenceFile;
}

}
