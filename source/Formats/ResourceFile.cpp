#include "Formats/ResourceFile.h"

#include "Formats/ByteReader.h"

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "Resource file";

// The root of a BRRES file is at the offset that the header keeps at 0x0C; its index group starts 8 bytes into it.
constexpr std::size_t RootOffsetPosition = 0x0C;
constexpr std::size_t RootGroupInsideRoot = 8;

// An index group: its size, the count of its entries, then entries of 16 bytes, the first of which is the root of the group. An entry
// keeps the offsets of its name and of its data, both from the start of the group.
constexpr std::size_t GroupCountPosition = 4;
constexpr std::size_t FirstGroupEntry = 8;
constexpr std::size_t GroupEntrySize = 16;
constexpr std::size_t EntryNamePosition = 8;
constexpr std::size_t EntryDataPosition = 12;

std::size_t OffsetFrom(std::size_t start, std::uint32_t relative)
{
    return start + static_cast<std::size_t>(static_cast<std::int32_t>(relative));
}

}

std::vector<ResourceEntry> ReadIndexGroup(ByteView bytes, std::size_t groupOffset)
{
    ByteReader reader(bytes, DataDescription);
    reader.MoveTo(groupOffset + GroupCountPosition);
    std::uint32_t count = reader.ReadUint32();

    std::vector<ResourceEntry> entries;
    for (std::uint32_t entryNumber = 1; entryNumber <= count; ++entryNumber)
    {
        reader.MoveTo(groupOffset + FirstGroupEntry + entryNumber * GroupEntrySize + EntryNamePosition);
        std::size_t nameOffset = OffsetFrom(groupOffset, reader.ReadUint32());

        reader.MoveTo(groupOffset + FirstGroupEntry + entryNumber * GroupEntrySize + EntryDataPosition);
        std::size_t dataOffset = OffsetFrom(groupOffset, reader.ReadUint32());

        reader.MoveTo(nameOffset);
        entries.push_back({reader.ReadTextUntilZero(), dataOffset});
    }
    return entries;
}

std::optional<std::size_t> FindResource(ByteView brres, std::string_view groupName, std::string_view entryName)
{
    ByteReader reader(brres, DataDescription);
    reader.MoveTo(RootOffsetPosition);
    std::size_t rootGroup = reader.ReadUint16() + RootGroupInsideRoot;

    for (const ResourceEntry& group : ReadIndexGroup(brres, rootGroup))
    {
        if (group.name != groupName)
            continue;

        for (const ResourceEntry& entry : ReadIndexGroup(brres, group.offset))
        {
            if (entry.name == entryName)
                return entry.offset;
        }
    }
    return std::nullopt;
}

}
