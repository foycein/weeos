#pragma once

#include "Mii/MiiRecord.h"

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Mii
{

constexpr std::size_t MiiSlotCount = 100;
constexpr std::size_t HiddenMiiCapacity = 10000;
constexpr std::size_t MiiDatabaseFileSize = 779968;

// The Mii database of the console, /shared2/menu/FaceLib/RFL_DB.dat (docs/research/Mii.md section 2): 100 Miis, the switch of the
// Mii Parade and the hidden Miis of the Mii Parade. The object holds the bytes of the file and reads and changes them in place,
// so whatever it does not change is written back exactly as it was read.
class MiiDatabase
{
public:
    // A new database as the format makes it: no Miis, no hidden Miis, the Mii Parade switched off.
    static MiiDatabase MakeEmpty();

    // Throws a FormatError when the file is shorter than the part its checksum covers or the checksum is wrong. The face library
    // then refuses the database until it is formatted, which the Mii Channel offers.
    static MiiDatabase Read(Formats::ByteView fileBytes);

    // The whole file, with a new checksum.
    Formats::Bytes FileBytes() const;

    // The Mii in a slot, or nothing when the slot is empty or its Mii cannot be used (invalid or temporary).
    std::optional<MiiRecord> FindMii(std::size_t slot) const;

    // The first slot without a usable Mii, or nothing when all slots are used.
    std::optional<std::size_t> FirstFreeSlot() const;

    // A Mii that was not made on this console may not travel. The Mii leaves the hidden list if it is there.
    void StoreMii(std::size_t slot, MiiRecord record, std::uint32_t consoleSystemId);

    void DeleteMii(std::size_t slot);

    bool IsMiiParadeOn() const;
    void SetMiiParadeOn(bool isOn);

    // The hidden Miis, oldest first. They have no creator name and no birthday.
    std::vector<MiiRecord> HiddenMiis() const;

    // A Mii joins the Mii Parade: it becomes the newest hidden Mii, without its creator name, birthday and favorite mark. The same Mii
    // leaves its older place in the list; when all 10,000 places are used, the oldest Mii gives its place. The Miis that other consoles
    // send come into the parade the same way.
    void AddHiddenMii(MiiRecord record);

    // Takes the hidden Mii at this place of HiddenMiis() out of the Mii Parade.
    void RemoveHiddenMii(std::size_t place);

    // The repair the Mii Channel runs at start: every Mii that has a create id but is invalid or temporary is removed,
    // from the slots and from the hidden list. Gives the number of Miis removed.
    std::size_t RemoveBrokenMiis();

private:
    struct HiddenEntry
    {
        std::uint64_t createId = 0;
        bool isFemale = false;
        std::uint16_t newerEntry = 0;
        std::uint16_t olderEntry = 0;
    };

    explicit MiiDatabase(Formats::Bytes fileBytes);

    std::span<const Formats::Byte, MiiRecordSize> SlotBytes(std::size_t slot) const;
    std::span<Formats::Byte, MiiRecordSize> SlotBytes(std::size_t slot);
    MiiRecord ReadSlot(std::size_t slot) const;

    std::uint16_t OldestHiddenEntry() const;
    std::uint16_t NewestHiddenEntry() const;
    std::optional<std::uint16_t> FreeHiddenEntry() const;
    HiddenEntry ReadHiddenEntry(std::uint16_t entryNumber) const;
    void WriteHiddenEntry(std::uint16_t entryNumber, const HiddenEntry& entry);
    MiiRecord ReadHiddenMii(std::uint16_t entryNumber) const;
    void WriteHiddenMii(std::uint16_t entryNumber, const MiiRecord& record);
    std::vector<std::uint16_t> HiddenEntriesInOrder() const;
    void RemoveHiddenEntry(std::uint16_t entryNumber);
    void RemoveFromHiddenList(std::uint64_t createId);

    Formats::Bytes bytes;
};

}
