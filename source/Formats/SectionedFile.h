#pragma once

#include "Formats/ByteReader.h"
#include "Formats/Bytes.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

constexpr std::size_t SectionHeaderSize = 8;

// Layouts, animations and fonts are files with a header and then sections. Each section starts
// with a tag of four letters and its own size.
struct Section
{
    std::string tag;

    // The whole section, including the eight bytes of the tag and the size.
    ByteView bytes;
};

struct SectionedFile
{
    std::uint16_t version;
    std::vector<Section> sections;
};

// Reads the header and cuts the file into its sections. The sections must fill the file exactly.
// The sections point into the given bytes, which must stay alive while the sections are used.
SectionedFile ReadSectionedFile(ByteView bytes, std::string_view expectedTag, std::string_view dataDescription);

// A reader for the section, positioned behind the tag and the size.
ByteReader OpenSection(const Section& section, std::string_view dataDescription);

// Walks through the sections one by one.
class SectionCursor
{
public:
    SectionCursor(const std::vector<Section>& sectionsToWalk, std::string_view dataDescription);

    bool IsAtEnd() const;
    bool NextIs(std::string_view tag) const;

    // Takes the next section, whatever it is.
    const Section& Take();

    // Takes the next section. It must have this tag.
    const Section& Take(std::string_view tag);

private:
    const std::vector<Section>& sections;
    std::string description;
    std::size_t nextIndex = 0;
};

}
