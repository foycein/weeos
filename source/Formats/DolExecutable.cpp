#include "Formats/DolExecutable.h"

#include "Formats/ByteReader.h"

#include <array>
#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t TextSectionCount = 7;
constexpr std::size_t DataSectionCount = 11;
constexpr std::size_t SectionSlotCount = TextSectionCount + DataSectionCount;
constexpr std::size_t HeaderSize = 0x100;

using SlotNumbers = std::array<std::uint32_t, SectionSlotCount>;

SlotNumbers ReadSlotNumbers(ByteReader& reader)
{
    SlotNumbers numbers;
    for (std::uint32_t& number : numbers)
        number = reader.ReadUint32();
    return numbers;
}

DolSection MakeSection(std::size_t slot, std::uint32_t fileOffset, std::uint32_t loadAddress, std::uint32_t size)
{
    DolSection section;
    section.kind = slot < TextSectionCount ? DolSectionKind::Text : DolSectionKind::Data;
    section.number = slot < TextSectionCount ? slot : slot - TextSectionCount;
    section.fileOffset = fileOffset;
    section.loadAddress = loadAddress;
    section.size = size;
    return section;
}

void CheckSectionIsInsideTheFile(const ByteReader& reader, const DolSection& section)
{
    std::string name = section.kind == DolSectionKind::Text ? "text" : "data";

    if (section.fileOffset < HeaderSize || !IsRangeInside(section.fileOffset, section.size, reader.Size()))
        reader.Fail(std::format("the {} section {} lies outside the file", name, section.number));
}

}

ByteView DolExecutable::SectionData(const DolSection& section) const&
{
    return ByteView(bytes).subspan(section.fileOffset, section.size);
}

DolExecutable ReadDolExecutable(ByteView bytes)
{
    ByteReader reader(bytes, "DOL executable");
    if (bytes.size() < HeaderSize)
        reader.Fail(std::format("the file has {:#x} bytes, but the header needs {:#x}", bytes.size(), HeaderSize));

    SlotNumbers fileOffsets = ReadSlotNumbers(reader);
    SlotNumbers loadAddresses = ReadSlotNumbers(reader);
    SlotNumbers sizes = ReadSlotNumbers(reader);

    DolExecutable executable;
    executable.bssAddress = reader.ReadUint32();
    executable.bssSize = reader.ReadUint32();
    executable.entryPoint = reader.ReadUint32();

    for (std::size_t slot = 0; slot < SectionSlotCount; ++slot)
    {
        if (sizes[slot] == 0)
            continue;

        DolSection section = MakeSection(slot, fileOffsets[slot], loadAddresses[slot], sizes[slot]);
        CheckSectionIsInsideTheFile(reader, section);
        executable.sections.push_back(section);
    }

    executable.bytes.assign(bytes.begin(), bytes.end());
    return executable;
}

}
