#pragma once

#include "Formats/Bytes.h"
#include "Ios/Title/TitleId.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace SystemMenu
{

// The channel grid has four pages of twelve places.
constexpr int PageCount = 4;
constexpr int SlotsPerPage = 12;
constexpr int SlotCount = PageCount * SlotsPerPage;

// What a place of the grid holds, as the number in the first byte of its record in iplsave.bin says.
enum class SavedSlotKind : std::uint8_t
{
    Empty = 0,
    DiscChannel = 1,
    Title = 3,
};

// The numbers behind the kind in a record. The menu calls a channel title 0x0E and the Disc Channel 0x0F.
constexpr std::uint32_t TitleItemNumber = 0x0E;
constexpr std::uint32_t DiscItemNumber = 0x0F;

// One place of the channel grid, as iplsave.bin keeps it. The menu's own table of default channels sets the flag to 1
// and a record that the menu writes itself has it 0. What the flag means is not known, so it is kept as it was read.
struct SavedSlot
{
    SavedSlotKind kind = SavedSlotKind::Empty;
    std::uint8_t flag = 0;
    std::uint32_t itemNumber = 0;
    Ios::TitleId titleId;

    bool operator==(const SavedSlot&) const = default;
};

// The bytes of iplsave.bin that the menu keeps as they are: what lies between the places and the digest.
constexpr std::size_t OtherDataSize = 0x1A0;

// The menu's own save file, /title/00000001/00000002/data/iplsave.bin (docs/audit/OpenPoints/SystemMenu.md SYS-01).
// It holds the places of the channel grid in the order of the pages. The page that the menu shows is not stored in the file
// while the menu runs. A channel that starts remembers its page here, so that the menu can open on it when the channel ends.
struct SaveData
{
    std::uint32_t version = 0;

    // A page that is not 0 is the page to show first. The menu reads it and sets it back to 0.
    std::uint32_t pageToReturnTo = 0;

    std::array<SavedSlot, SlotCount> slots;
    std::array<Formats::Byte, OtherDataSize> otherData = {};
};

// The version that this menu writes. A file with a version below it is converted, a file with a higher one is not understood.
constexpr std::uint32_t CurrentSaveDataVersion = 3;

// The file of a new console: the Disc Channel and the five channels that the menu expects, Mii, Photo, Shop, Forecast and News,
// as the original writes it when it finds no file or a damaged one.
SaveData MakeDefaultSaveData();

// Reads a file. Gives nothing when the file is damaged: it does not start with RIPL, its size is wrong, its version is one that
// this menu does not know or the MD5 digest at its end is not the digest of the bytes before it.
// A file of an older version is converted to the current one.
std::optional<SaveData> ReadSaveData(Formats::ByteView bytes);

// The bytes of the file, with the digest.
Formats::Bytes WriteSaveData(const SaveData& saveData);

// Before a channel starts, the menu puts it at the front of the list of started titles in the other data (0x813596B8), when it is
// a title of the kinds 0x00010001, 3, 4 or 6 and not the Internet Channel.
void PutFirstInTheListOfStartedTitles(SaveData& saveData, Ios::TitleId titleId);

// The SD Card Menu explains itself on four pages the first time it opens; a new file says that it has not yet (0x813DDE90).
bool IsSdCardMenuIntroPending(const SaveData& saveData);
void MarkSdCardMenuIntroSeen(SaveData& saveData);

}
