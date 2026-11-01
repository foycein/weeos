#include "SystemMenu/WiiMenu/SaveData.h"

#include "SystemMenu/WiiMenu/ChannelTitleIds.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/Md5.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr std::array<Formats::Byte, 4> Magic = {'R', 'I', 'P', 'L'};
constexpr std::size_t NumberSize = sizeof(std::uint32_t);

// The file has a header of 16 bytes (the magic, the size of the file, the version and the page to return to),
// the 48 places of 16 bytes, the other data and, in the last 16 bytes, the MD5 digest of everything before it.
constexpr std::size_t FileSize = 0x4C0;
constexpr std::size_t HeaderSize = 0x10;
constexpr std::size_t SlotRecordPaddingSize = 2;
constexpr std::size_t OtherDataOffset = HeaderSize + SlotCount * 0x10;
constexpr std::size_t DigestOffset = FileSize - Formats::Md5DigestSize;
static_assert(OtherDataOffset + OtherDataSize == DigestOffset, "the other data fills the space between the places and the digest");

// The smallest size a file can declare. A file of an older version may be shorter than the current one.
constexpr std::size_t SmallestFileSize = 0x20;

// Inside the other data. The menu writes the same numbers into a new file and clears the same parts when it converts an older file.
// What these numbers mean was not found out, so they are kept as they were read. The list holds the titles that were started,
// the last one first (0x813596B8).
constexpr std::size_t UnknownBlockOffset = 0;
constexpr std::array<Formats::Byte, 8> UnknownBlock = {0x11, 0xFA, 0x00, 0x00, 0x90, 0x10, 0x00, 0x00};
constexpr std::size_t FirstNumberOffset = 0x0C;
constexpr Formats::Byte FirstNumberAtStart = 1;
constexpr std::size_t TitleListOffset = 0x10;
constexpr std::size_t TitleListSize = 0x180;
constexpr std::size_t TitleIdSize = sizeof(std::uint64_t);
constexpr std::size_t TitleListLength = TitleListSize / TitleIdSize;

// Only titles of these kinds go into the list of started titles, and not the Internet Channel (HADE, and HATE of the test units).
constexpr std::array<std::uint32_t, 4> KindsInTheListOfStartedTitles = {0x00010001, 0x00010003, 0x00010004, 0x00010006};
constexpr std::array<Ios::TitleId, 2> TitlesLeftOutOfTheList = {{{0x00010001, 0x48414445}, {0x00010001, 0x48415445}}};
constexpr std::size_t SecondNumberOffset = 0x190;

// The files of the versions 0 and 1 lack the block of eight bytes, the files of the versions 0 to 2 lack the list.
constexpr std::uint32_t FirstVersionWithUnknownBlock = 2;

// The places of the Disc Channel and of the five channels that the menu expects on a new console.
constexpr std::size_t DiscChannelSlot = 0;
constexpr std::size_t FirstDefaultTitleSlot = 1;
constexpr std::array<std::uint32_t, 5> DefaultTitleLowParts = {
    0x48414341, // HACA, the Mii Channel
    0x48414141, // HAAA, the Photo Channel
    0x48414241, // HABA, the Wii Shop Channel
    0x48414641, // HAFA, the Forecast Channel
    0x48414741, // HAGA, the News Channel
};

SavedSlot MakeDefaultTitleSlot(std::uint32_t lowPart)
{
    SavedSlot slot;
    slot.kind = SavedSlotKind::Title;
    slot.flag = 1;
    slot.itemNumber = TitleItemNumber;
    slot.titleId = {SystemChannelKind, lowPart};
    return slot;
}

SavedSlot MakeDiscSlot()
{
    SavedSlot slot;
    slot.kind = SavedSlotKind::DiscChannel;
    slot.flag = 1;
    slot.itemNumber = DiscItemNumber;
    return slot;
}

void WriteUnknownBlock(std::array<Formats::Byte, OtherDataSize>& otherData)
{
    std::copy(UnknownBlock.begin(), UnknownBlock.end(), otherData.begin() + UnknownBlockOffset);
}

void ClearTitleList(std::array<Formats::Byte, OtherDataSize>& otherData)
{
    std::fill_n(otherData.begin() + FirstNumberOffset, NumberSize, Formats::Byte(0));
    otherData[FirstNumberOffset + NumberSize - 1] = FirstNumberAtStart;

    std::fill_n(otherData.begin() + TitleListOffset, TitleListSize, Formats::Byte(0));
    std::fill_n(otherData.begin() + SecondNumberOffset, NumberSize, Formats::Byte(0));
}

std::uint64_t ReadListedTitle(const std::array<Formats::Byte, OtherDataSize>& otherData, std::size_t place)
{
    std::uint64_t value = 0;
    for (std::size_t byte = 0; byte < TitleIdSize; ++byte)
        value = (value << 8) | otherData[TitleListOffset + place * TitleIdSize + byte];

    return value;
}

void WriteListedTitle(std::array<Formats::Byte, OtherDataSize>& otherData, std::size_t place, std::uint64_t value)
{
    for (std::size_t byte = 0; byte < TitleIdSize; ++byte)
    {
        std::size_t shift = 8 * (TitleIdSize - 1 - byte);
        otherData[TitleListOffset + place * TitleIdSize + byte] = static_cast<Formats::Byte>(value >> shift);
    }
}

bool BelongsInTheListOfStartedTitles(Ios::TitleId titleId)
{
    bool isLeftOut = std::find(TitlesLeftOutOfTheList.begin(), TitlesLeftOutOfTheList.end(), titleId) != TitlesLeftOutOfTheList.end();
    if (isLeftOut)
        return false;

    return std::find(KindsInTheListOfStartedTitles.begin(), KindsInTheListOfStartedTitles.end(), titleId.highPart) !=
        KindsInTheListOfStartedTitles.end();
}

SavedSlot ReadSlot(Formats::ByteReader& reader)
{
    SavedSlot slot;
    slot.kind = static_cast<SavedSlotKind>(reader.ReadByte());
    slot.flag = reader.ReadByte();
    reader.Skip(SlotRecordPaddingSize);
    slot.itemNumber = reader.ReadUint32();
    slot.titleId = Ios::TitleId::FromValue(reader.ReadUint64());
    return slot;
}

void WriteSlot(Formats::ByteWriter& writer, const SavedSlot& slot)
{
    writer.WriteByte(static_cast<Formats::Byte>(slot.kind));
    writer.WriteByte(slot.flag);
    writer.WriteZeros(SlotRecordPaddingSize);
    writer.WriteUint32(slot.itemNumber);
    writer.WriteUint64(slot.titleId.ToValue());
}

// The digest in the last 16 bytes of the file is the digest of all the bytes before it.
bool HasMatchingDigest(Formats::ByteView bytes, std::size_t size)
{
    std::size_t digestOffset = size - Formats::Md5DigestSize;
    Formats::Md5Digest digest = Formats::ComputeMd5(bytes.subspan(0, digestOffset));
    return std::equal(digest.begin(), digest.end(), bytes.begin() + digestOffset);
}

// The file as a block of the current size. A file of an older version may be shorter, and what it lacks is zero.
Formats::Bytes ExtendToCurrentSize(Formats::ByteView bytes, std::size_t size)
{
    Formats::Bytes extended(FileSize, 0);
    std::size_t contentSize = std::min(size - Formats::Md5DigestSize, DigestOffset);
    std::copy_n(bytes.begin(), contentSize, extended.begin());
    return extended;
}

}

SaveData MakeDefaultSaveData()
{
    SaveData saveData;
    saveData.version = CurrentSaveDataVersion;
    saveData.slots[DiscChannelSlot] = MakeDiscSlot();
    for (std::size_t index = 0; index < DefaultTitleLowParts.size(); ++index)
        saveData.slots[FirstDefaultTitleSlot + index] = MakeDefaultTitleSlot(DefaultTitleLowParts[index]);

    WriteUnknownBlock(saveData.otherData);
    ClearTitleList(saveData.otherData);
    return saveData;
}

std::optional<SaveData> ReadSaveData(Formats::ByteView bytes)
{
    if (bytes.size() < SmallestFileSize)
        return std::nullopt;

    Formats::ByteReader reader(bytes, "iplsave.bin");
    if (reader.ReadByteArray<Magic.size()>() != Magic)
        return std::nullopt;

    std::uint32_t size = reader.ReadUint32();
    std::uint32_t version = reader.ReadUint32();
    if (size < SmallestFileSize || size > bytes.size() || version > CurrentSaveDataVersion)
        return std::nullopt;

    bool isCurrentVersion = version == CurrentSaveDataVersion;
    if (isCurrentVersion && size != FileSize)
        return std::nullopt;

    if (!HasMatchingDigest(bytes, size))
        return std::nullopt;

    Formats::Bytes content = ExtendToCurrentSize(bytes, size);
    Formats::ByteReader contentReader(content, "iplsave.bin");
    contentReader.Skip(HeaderSize - NumberSize);

    SaveData saveData;
    saveData.version = CurrentSaveDataVersion;
    saveData.pageToReturnTo = contentReader.ReadUint32();
    for (SavedSlot& slot : saveData.slots)
        slot = ReadSlot(contentReader);

    std::copy_n(content.begin() + OtherDataOffset, OtherDataSize, saveData.otherData.begin());

    if (version < FirstVersionWithUnknownBlock)
        WriteUnknownBlock(saveData.otherData);

    if (!isCurrentVersion)
        ClearTitleList(saveData.otherData);

    return saveData;
}

Formats::Bytes WriteSaveData(const SaveData& saveData)
{
    Formats::ByteWriter writer;
    writer.WriteBytes(Magic);
    writer.WriteUint32(FileSize);
    writer.WriteUint32(saveData.version);
    writer.WriteUint32(saveData.pageToReturnTo);
    for (const SavedSlot& slot : saveData.slots)
        WriteSlot(writer, slot);

    writer.WriteBytes(saveData.otherData);

    Formats::Md5Digest digest = Formats::ComputeMd5(writer.GetBytes());
    writer.WriteBytes(digest);
    return writer.GetBytes();
}

// The title moves to the front; the titles before its old place move one place back. A title that was not in the list pushes the last one out.
void PutFirstInTheListOfStartedTitles(SaveData& saveData, Ios::TitleId titleId)
{
    if (!BelongsInTheListOfStartedTitles(titleId))
        return;

    std::size_t oldPlace = 0;
    while (oldPlace < TitleListLength - 1 && ReadListedTitle(saveData.otherData, oldPlace) != titleId.ToValue())
        ++oldPlace;

    for (std::size_t place = oldPlace; place > 0; --place)
        WriteListedTitle(saveData.otherData, place, ReadListedTitle(saveData.otherData, place - 1));

    WriteListedTitle(saveData.otherData, 0, titleId.ToValue());
}

}
