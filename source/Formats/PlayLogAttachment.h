#pragma once

#include "Formats/Bytes.h"
#include "Formats/PlayRecordFile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Formats
{

constexpr std::size_t PlayLogSlotCount = 10;
constexpr std::size_t PlayLogNameCount = 10;
constexpr std::size_t PlayLogNameLength = 12;
constexpr std::size_t PlayLogAttachmentSize = 0x668;

// The attachment of "Today's Accomplishments" (the play log of a day, 0x81341550 to 0x81341858): ten places for the play records of
// the day, the time of the titles that found no place, and ten names of people the console sent messages to. Every play gets a place
// of its own, also when the same title was played before on that day.
struct PlayLogAttachment
{
    // The four bytes after "03_0" are never written by the menu.
    std::uint32_t unusedWord = 0;

    std::array<std::optional<PlayRecord>, PlayLogSlotCount> plays;

    // Ticks of the console's timer that the titles played after the ten places were full ran.
    std::uint64_t ticksOfOtherTitles = 0;

    std::array<std::optional<std::array<char16_t, PlayLogNameLength>>, PlayLogNameCount> namesSentTo;

    // How many names found no place.
    std::uint32_t moreNamesSentTo = 0;
};

// Also reads the older form of 0x558 bytes, which has only the ten places and the time of the other titles.
// Throws a FormatError for another size, another magic or a play record whose sum is wrong.
PlayLogAttachment ReadPlayLogAttachment(ByteView bytes);

// Always writes the form of 0x668 bytes.
Bytes WritePlayLogAttachment(const PlayLogAttachment& playLog);

}
