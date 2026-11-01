#include "Mii/MiiDatabase.h"

#include "Mii/MiiChecksum.h"

#include "Formats/BitFields.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Mii
{

namespace
{

constexpr std::uint32_t DatabaseTag = 0x524E4F44;
constexpr std::uint32_t HiddenListTag = 0x524E4844;

constexpr std::size_t DatabaseTagOffset = 0x00000;
constexpr std::size_t FirstSlotOffset = 0x00004;
constexpr std::size_t ParadeSwitchOffset = 0x01CEC;
constexpr std::size_t HiddenListTagOffset = 0x01D00;
constexpr std::size_t OldestHiddenEntryOffset = 0x01D04;
constexpr std::size_t NewestHiddenEntryOffset = 0x01D06;
constexpr std::size_t FirstHiddenEntryOffset = 0x01D08;
constexpr std::size_t ChecksumOffset = 0x1F1DE;
constexpr std::size_t CheckedPartSize = 0x1F1E0;
constexpr std::size_t FirstHiddenMiiOffset = 0x1F1E0;

constexpr std::size_t HiddenEntrySize = 12;
constexpr std::size_t CreateIdSize = 8;
constexpr std::size_t HiddenMiiSize = 0x40;

// A hidden Mii keeps the record up to the creator name.
constexpr std::size_t HiddenMiiRecordPart = 0x36;

// Bit 31 of the switch word set means that the Mii Parade is off.
constexpr std::uint32_t ParadeOffFlag = 0x80000000;

// The list of hidden Miis names its ends with 0xFFFF when it is empty, and an entry names a missing neighbor with 0x7FFF.
constexpr std::uint16_t NoListEnd = 0xFFFF;
constexpr std::uint16_t NoNeighbor = 0x7FFF;
constexpr int NeighborBitCount = 15;
constexpr int GenderBit = 15;

// A create id is the Mii id followed by the system id.
constexpr int BitsPerId = 32;

std::uint32_t ReadNumber(const Formats::Bytes& bytes, std::size_t offset, std::size_t byteCount)
{
    std::uint32_t number = 0;
    for (std::size_t index = 0; index < byteCount; ++index)
        number = (number << Formats::BitsPerByte) | bytes[offset + index];
    return number;
}

void WriteNumber(Formats::Bytes& bytes, std::size_t offset, std::size_t byteCount, std::uint32_t number)
{
    for (std::size_t index = 0; index < byteCount; ++index)
    {
        std::size_t shift = (byteCount - 1 - index) * Formats::BitsPerByte;
        bytes[offset + index] = static_cast<Formats::Byte>(number >> shift);
    }
}

std::uint64_t CreateIdOf(const MiiRecord& record)
{
    return (static_cast<std::uint64_t>(record.miiId) << BitsPerId) | record.systemId;
}

// What the list stores as its oldest or newest entry when the neighbor of a removed entry takes that place.
std::uint16_t ListEndFrom(std::uint16_t neighbor)
{
    if (neighbor == NoNeighbor)
        return NoListEnd;

    return neighbor;
}

void RequireSlot(std::size_t slot)
{
    if (slot >= MiiSlotCount)
        throw Formats::FormatError(std::format("Mii database: there is no slot {}, the slots are 0 to {}", slot, MiiSlotCount - 1));
}

std::size_t HiddenEntryOffset(std::uint16_t entryNumber)
{
    return FirstHiddenEntryOffset + entryNumber * HiddenEntrySize;
}

}

MiiDatabase::MiiDatabase(Formats::Bytes fileBytes)
    : bytes(std::move(fileBytes))
{
}

MiiDatabase MiiDatabase::MakeEmpty()
{
    Formats::Bytes fileBytes(MiiDatabaseFileSize, 0);
    WriteNumber(fileBytes, DatabaseTagOffset, sizeof(std::uint32_t), DatabaseTag);
    WriteNumber(fileBytes, ParadeSwitchOffset, sizeof(std::uint32_t), ParadeOffFlag);
    WriteNumber(fileBytes, HiddenListTagOffset, sizeof(std::uint32_t), HiddenListTag);
    WriteNumber(fileBytes, OldestHiddenEntryOffset, sizeof(std::uint16_t), NoListEnd);
    WriteNumber(fileBytes, NewestHiddenEntryOffset, sizeof(std::uint16_t), NoListEnd);

    MiiDatabase database(std::move(fileBytes));
    for (std::size_t entryNumber = 0; entryNumber < HiddenMiiCapacity; ++entryNumber)
        database.WriteHiddenEntry(static_cast<std::uint16_t>(entryNumber), HiddenEntry{0, false, NoNeighbor, NoNeighbor});

    return database;
}

MiiDatabase MiiDatabase::Read(Formats::ByteView fileBytes)
{
    if (fileBytes.size() < CheckedPartSize)
    {
        throw Formats::FormatError(
            std::format("Mii database: the file has {} bytes, fewer than the {} its checksum covers", fileBytes.size(), CheckedPartSize));
    }

    Formats::Bytes completeBytes(fileBytes.begin(), fileBytes.end());
    auto storedChecksum = static_cast<std::uint16_t>(ReadNumber(completeBytes, ChecksumOffset, sizeof(std::uint16_t)));
    std::uint16_t checksum = CalculateMiiChecksum(fileBytes.first(ChecksumOffset));
    if (checksum != storedChecksum)
    {
        throw Formats::FormatError(
            std::format("Mii database: the checksum is {:#06x} but the file says {:#06x}", checksum, storedChecksum));
    }

    // A file that ends early is read as if the missing hidden Miis were zero, which is what the console's buffer holds then.
    if (completeBytes.size() < MiiDatabaseFileSize)
        completeBytes.resize(MiiDatabaseFileSize, 0);

    return MiiDatabase(std::move(completeBytes));
}

Formats::Bytes MiiDatabase::FileBytes() const
{
    Formats::Bytes fileBytes = bytes;
    std::uint16_t checksum = CalculateMiiChecksum(Formats::ByteView(fileBytes).first(ChecksumOffset));
    WriteNumber(fileBytes, ChecksumOffset, sizeof(std::uint16_t), checksum);
    return fileBytes;
}

std::optional<MiiRecord> MiiDatabase::FindMii(std::size_t slot) const
{
    RequireSlot(slot);

    MiiRecord record = ReadSlot(slot);
    if (!IsUsableMii(record))
        return std::nullopt;

    return record;
}

std::optional<std::size_t> MiiDatabase::FirstFreeSlot() const
{
    for (std::size_t slot = 0; slot < MiiSlotCount; ++slot)
    {
        if (!FindMii(slot))
            return slot;
    }
    return std::nullopt;
}

void MiiDatabase::StoreMii(std::size_t slot, MiiRecord record, std::uint32_t consoleSystemId)
{
    RequireSlot(slot);

    if (!WasMadeOnConsole(record, consoleSystemId))
        record.mayNotTravel = true;

    WriteMiiRecord(record, SlotBytes(slot));
    RemoveFromHiddenList(CreateIdOf(record));
}

void MiiDatabase::DeleteMii(std::size_t slot)
{
    RequireSlot(slot);

    std::span<Formats::Byte, MiiRecordSize> slotBytes = SlotBytes(slot);
    std::fill(slotBytes.begin(), slotBytes.end(), Formats::Byte(0));
}

bool MiiDatabase::IsMiiParadeOn() const
{
    std::uint32_t switchWord = ReadNumber(bytes, ParadeSwitchOffset, sizeof(std::uint32_t));
    return (switchWord & ParadeOffFlag) == 0;
}

void MiiDatabase::SetMiiParadeOn(bool isOn)
{
    std::uint32_t switchWord = ReadNumber(bytes, ParadeSwitchOffset, sizeof(std::uint32_t));
    if (isOn)
        switchWord &= ~ParadeOffFlag;
    else
        switchWord |= ParadeOffFlag;
    WriteNumber(bytes, ParadeSwitchOffset, sizeof(std::uint32_t), switchWord);
}

std::vector<MiiRecord> MiiDatabase::HiddenMiis() const
{
    std::vector<MiiRecord> hiddenMiis;
    for (std::uint16_t entryNumber : HiddenEntriesInOrder())
        hiddenMiis.push_back(ReadHiddenMii(entryNumber));
    return hiddenMiis;
}

std::size_t MiiDatabase::RemoveBrokenMiis()
{
    std::size_t removedCount = 0;

    for (std::size_t slot = 0; slot < MiiSlotCount; ++slot)
    {
        MiiRecord record = ReadSlot(slot);
        bool isBroken = !HasEmptyCreateId(record) && (!IsValidMii(record) || IsTemporaryMii(record));
        if (!isBroken)
            continue;

        DeleteMii(slot);
        ++removedCount;
    }

    for (std::uint16_t entryNumber : HiddenEntriesInOrder())
    {
        MiiRecord record = ReadHiddenMii(entryNumber);
        if (IsValidMii(record) && !IsTemporaryMii(record))
            continue;

        RemoveHiddenEntry(entryNumber);
        ++removedCount;
    }

    return removedCount;
}

std::span<const Formats::Byte, MiiRecordSize> MiiDatabase::SlotBytes(std::size_t slot) const
{
    return std::span<const Formats::Byte, MiiRecordSize>(bytes.data() + FirstSlotOffset + slot * MiiRecordSize, MiiRecordSize);
}

std::span<Formats::Byte, MiiRecordSize> MiiDatabase::SlotBytes(std::size_t slot)
{
    return std::span<Formats::Byte, MiiRecordSize>(bytes.data() + FirstSlotOffset + slot * MiiRecordSize, MiiRecordSize);
}

MiiRecord MiiDatabase::ReadSlot(std::size_t slot) const
{
    return ReadMiiRecord(SlotBytes(slot));
}

std::uint16_t MiiDatabase::OldestHiddenEntry() const
{
    return static_cast<std::uint16_t>(ReadNumber(bytes, OldestHiddenEntryOffset, sizeof(std::uint16_t)));
}

MiiDatabase::HiddenEntry MiiDatabase::ReadHiddenEntry(std::uint16_t entryNumber) const
{
    std::size_t offset = HiddenEntryOffset(entryNumber);
    std::uint32_t newerWord = ReadNumber(bytes, offset + CreateIdSize, sizeof(std::uint16_t));
    std::uint32_t olderWord = ReadNumber(bytes, offset + CreateIdSize + sizeof(std::uint16_t), sizeof(std::uint16_t));

    HiddenEntry entry;
    std::uint64_t miiId = ReadNumber(bytes, offset, sizeof(std::uint32_t));
    std::uint64_t systemId = ReadNumber(bytes, offset + sizeof(std::uint32_t), sizeof(std::uint32_t));
    entry.createId = (miiId << BitsPerId) | systemId;
    entry.isFemale = Formats::ExtractBits(newerWord, GenderBit, 1) != 0;
    entry.newerEntry = static_cast<std::uint16_t>(Formats::ExtractBits(newerWord, 0, NeighborBitCount));
    entry.olderEntry = static_cast<std::uint16_t>(Formats::ExtractBits(olderWord, 0, NeighborBitCount));
    return entry;
}

void MiiDatabase::WriteHiddenEntry(std::uint16_t entryNumber, const HiddenEntry& entry)
{
    std::size_t offset = HiddenEntryOffset(entryNumber);
    std::uint32_t genderFlag = entry.isFemale ? 1u << GenderBit : 0u;

    WriteNumber(bytes, offset, sizeof(std::uint32_t), static_cast<std::uint32_t>(entry.createId >> BitsPerId));
    WriteNumber(bytes, offset + sizeof(std::uint32_t), sizeof(std::uint32_t), static_cast<std::uint32_t>(entry.createId));
    WriteNumber(bytes, offset + CreateIdSize, sizeof(std::uint16_t), genderFlag | entry.newerEntry);
    WriteNumber(bytes, offset + CreateIdSize + sizeof(std::uint16_t), sizeof(std::uint16_t), entry.olderEntry);
}

MiiRecord MiiDatabase::ReadHiddenMii(std::uint16_t entryNumber) const
{
    std::array<Formats::Byte, MiiRecordSize> recordBytes = {};
    std::size_t offset = FirstHiddenMiiOffset + entryNumber * HiddenMiiSize;
    std::copy_n(bytes.begin() + offset, HiddenMiiRecordPart, recordBytes.begin());
    return ReadMiiRecord(recordBytes);
}

std::vector<std::uint16_t> MiiDatabase::HiddenEntriesInOrder() const
{
    std::vector<std::uint16_t> entryNumbers;

    std::uint16_t entryNumber = OldestHiddenEntry();
    if (entryNumber == NoListEnd)
        return entryNumbers;

    while (entryNumber != NoNeighbor)
    {
        if (entryNumber >= HiddenMiiCapacity || entryNumbers.size() >= HiddenMiiCapacity)
            throw Formats::FormatError("Mii database: the list of hidden Miis is broken");

        entryNumbers.push_back(entryNumber);
        entryNumber = ReadHiddenEntry(entryNumber).newerEntry;
    }
    return entryNumbers;
}

void MiiDatabase::RemoveHiddenEntry(std::uint16_t entryNumber)
{
    HiddenEntry entry = ReadHiddenEntry(entryNumber);

    if (entry.olderEntry == NoNeighbor)
    {
        WriteNumber(bytes, OldestHiddenEntryOffset, sizeof(std::uint16_t), ListEndFrom(entry.newerEntry));
    }
    else
    {
        HiddenEntry older = ReadHiddenEntry(entry.olderEntry);
        older.newerEntry = entry.newerEntry;
        WriteHiddenEntry(entry.olderEntry, older);
    }

    if (entry.newerEntry == NoNeighbor)
    {
        WriteNumber(bytes, NewestHiddenEntryOffset, sizeof(std::uint16_t), ListEndFrom(entry.olderEntry));
    }
    else
    {
        HiddenEntry newer = ReadHiddenEntry(entry.newerEntry);
        newer.olderEntry = entry.olderEntry;
        WriteHiddenEntry(entry.newerEntry, newer);
    }

    WriteHiddenEntry(entryNumber, HiddenEntry{0, false, NoNeighbor, NoNeighbor});
}

void MiiDatabase::RemoveFromHiddenList(std::uint64_t createId)
{
    for (std::uint16_t entryNumber : HiddenEntriesInOrder())
    {
        if (ReadHiddenEntry(entryNumber).createId == createId)
            RemoveHiddenEntry(entryNumber);
    }
}

}
