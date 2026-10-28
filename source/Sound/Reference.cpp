#include "Sound/Reference.h"

#include <format>
#include <vector>

namespace Sound
{

namespace
{

constexpr std::uint8_t NoPointerKind = 0;
constexpr std::uint8_t OffsetKind = 1;
constexpr std::size_t UnusedAfterTypeSize = 2;

}

bool Reference::IsNull() const
{
    return kind == NoPointerKind && offset == 0;
}

Reference ReadReference(Formats::ByteReader& reader)
{
    Reference reference;
    reference.kind = reader.ReadByte();
    reference.type = reader.ReadByte();
    reader.Skip(UnusedAfterTypeSize);
    reference.offset = reader.ReadUint32();
    return reference;
}

void MoveToTarget(Formats::ByteReader& reader, const Reference& reference, std::size_t baseOffset)
{
    if (reference.kind != OffsetKind)
        reader.Fail(std::format("a reference of the kind {} with the offset {:#x} cannot be followed", reference.kind, reference.offset));

    reader.MoveTo(baseOffset + reference.offset);
}

std::vector<Reference> ReadReferenceTable(Formats::ByteReader& reader)
{
    std::uint32_t count = reader.ReadUint32();

    std::vector<Reference> references;
    for (std::uint32_t index = 0; index < count; ++index)
        references.push_back(ReadReference(reader));

    return references;
}

}
