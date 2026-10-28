#pragma once

#include "Formats/Bytes.h"
#include "Ios/WiiConnect24/Nwc24Files.h"

#include <chrono>
#include <cstdint>

namespace Ios
{

// The date and the time of a file on a FAT file system.
struct FatTimestamp
{
    // Bits 15 to 9: the year since 1980, bits 8 to 5: the month, bits 4 to 0: the day.
    std::uint16_t date;

    // Bits 15 to 11: the hour, bits 10 to 5: the minute, bits 4 to 0: the seconds divided by 2.
    std::uint16_t time;

    // The time is exact to 2 seconds. This is the rest in hundredths of a second, 0 to 199.
    std::uint8_t hundredthsOfSecond;

    bool operator==(const FatTimestamp&) const = default;
};

// The FAT date and time of a moment in UTC. A moment before 1980 is the first of January 1980.
FatTimestamp MakeFatTimestamp(std::chrono::system_clock::time_point moment);

// The file of a mailbox (wc24send.mbx, wc24recv.mbx) as it is when the SDK has just made it: a volume of the size of the mailbox
// (a "VFF" file: a header, two copies of the FAT, a root directory of 0x1000 bytes and the clusters of 0x200 bytes)
// in which the directory /mb exists and is empty. The send mailbox is small enough for a FAT of 12 bits, the receive mailbox
// has one of 16 bits. Everything is made at the time of the timestamp.
Formats::Bytes MakeMailboxVolume(Mailbox mailbox, const FatTimestamp& timestamp);

// True when a file is a volume of the right size: it is as long as the mailbox needs and starts with the VFF header,
// in which the size is the length of the file. The first bytes of the file are enough to see it.
bool IsMailboxVolume(Formats::ByteView firstBytes, std::uint64_t fileLength, Mailbox mailbox);

}
