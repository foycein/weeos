#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <initializer_list>
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

// Reads the header and cuts out the sections. The sections must have exactly the expected tags.
SoundFile ReadSoundFileHeader(Formats::ByteView bytes, std::string_view expectedTag, std::uint16_t expectedVersion,
    std::size_t expectedHeaderSize, std::initializer_list<std::string_view> expectedSectionTags, std::string_view dataDescription);

}
