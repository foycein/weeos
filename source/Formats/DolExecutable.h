#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Formats
{

enum class DolSectionKind
{
    Text,
    Data,
};

// One block of the program that is loaded into memory. A DOL has up to 7 text sections (code) and 11 data sections.
struct DolSection
{
    DolSectionKind kind;

    // Counts the sections of the same kind from zero: text section 0, data section 3, and so on.
    std::size_t number;

    std::size_t fileOffset;
    std::uint32_t loadAddress;
    std::size_t size;
};

// A DOL file is the program format of the GameCube and the Wii. This reads the description of the program.
struct DolExecutable
{
    Bytes bytes;

    // Only the sections that are not empty, the text sections first.
    std::vector<DolSection> sections;

    // The memory that must be cleared before the program runs.
    std::uint32_t bssAddress;
    std::uint32_t bssSize;

    std::uint32_t entryPoint;

    // The data of a section stays inside the executable, so it is only valid while the executable exists.
    ByteView SectionData(const DolSection& section) const&;
    ByteView SectionData(const DolSection& section) const&& = delete;
};

DolExecutable ReadDolExecutable(ByteView bytes);

}
