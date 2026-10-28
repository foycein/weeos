#pragma once

#include "Formats/ByteReader.h"

#include <cstdint>
#include <vector>

namespace Sound
{

// A pointer inside a sound file. It is eight bytes: a kind, a type that says what lies at the target, and an offset.
// The kind is 1 for an offset that counts from the start of the data of the section, and 0 for no pointer at all.
struct Reference
{
    std::uint8_t kind;
    std::uint8_t type;
    std::uint32_t offset;

    bool IsNull() const;
};

Reference ReadReference(Formats::ByteReader& reader);

// Moves the reader to the target of the reference. The offset counts from baseOffset.
// A reference with no target or with a kind that is not known is a FormatError.
void MoveToTarget(Formats::ByteReader& reader, const Reference& reference, std::size_t baseOffset);

// The files hold lists of references in front of a count. Reads the count and the references.
std::vector<Reference> ReadReferenceTable(Formats::ByteReader& reader);

}
