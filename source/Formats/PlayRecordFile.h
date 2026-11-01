#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Formats
{

constexpr std::size_t PlayRecordFileSize = 0x80;

// The name has room for the two lines of the name in the banner of the title, 21 characters each, filled up with zeros.
constexpr std::size_t PlayRecordNameLineLength = 21;
constexpr std::size_t PlayRecordNameLength = 2 * PlayRecordNameLineLength;

constexpr std::size_t PlayRecordGameCodeSize = 4;
constexpr std::size_t PlayRecordMakerCodeSize = 2;
constexpr std::size_t PlayRecordSpareSize = 18;

// The file /title/00000001/00000002/data/play_rec.dat: the title that was started last and how long it ran. The menu writes it just
// before it starts a title, the title moves the last time on while it runs, and the play log of the Message Board reads it.
struct PlayRecord
{
    std::array<char16_t, PlayRecordNameLength> name;

    // Ticks of the timer of the console (a quarter of the bus clock) since the start of the year 2000.
    std::uint64_t startTime;
    std::uint64_t lastTime;

    // The four letters of the title, for example "HAYA", and the two letters of its maker.
    std::array<Byte, PlayRecordGameCodeSize> gameCode;
    std::array<Byte, PlayRecordMakerCodeSize> makerCode;

    // Zero when the console writes the file. Kept as it is.
    std::array<Byte, PlayRecordSpareSize> spare;
};

// The first four bytes are the sum of the 31 words that follow. Throws a FormatError when the size or the sum is wrong.
PlayRecord ReadPlayRecord(ByteView bytes);

// Writes the sum.
Bytes WritePlayRecord(const PlayRecord& record);

}
