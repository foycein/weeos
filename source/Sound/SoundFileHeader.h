#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Sound
{

// The sound files (archive, sequence, bank, wave sounds) start with the same header: a tag, the byte order mark,
// a version, the file size, the size of the header and a list of the sections with their offsets and sizes.
struct SoundFile
{
    std::uint16_t version;

    // Each section as a whole, including its tag and size, in the order the header lists them.
    std::vector<Formats::ByteView> sections;
};

// A version of a file type that is known, and the tags of the sections that files of this version have, in order.
struct KnownVersion
{
    std::uint16_t version;
    std::vector<std::string_view> sectionTags;
};

// Reads the header and cuts out the sections. The version must be one of the known ones, and the sections must have exactly
// the tags that go with it.
SoundFile ReadSoundFileHeader(Formats::ByteView bytes, std::string_view expectedTag, std::span<const KnownVersion> knownVersions,
    std::size_t expectedHeaderSize, std::string_view dataDescription);

}
