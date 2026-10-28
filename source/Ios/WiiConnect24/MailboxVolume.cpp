#include "Ios/WiiConnect24/MailboxVolume.h"

#include "Formats/ByteReader.h"
#include "Ios/BigEndianBytes.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace Ios
{

namespace
{

// The header of the VFF file. Its numbers are big endian like everything of the Wii. The FAT file system behind it keeps its
// numbers little endian, as every FAT file system does.
constexpr std::uint32_t VffMagic = 0x56464620;
constexpr std::uint16_t ByteOrderMark = 0xFEFF;
constexpr std::uint16_t VffVersion = 0x0100;
constexpr std::size_t ByteOrderMarkOffset = 4;
constexpr std::size_t VersionOffset = 6;
constexpr std::size_t VolumeSizeOffset = 8;
constexpr std::size_t ClusterSizeFieldOffset = 0x0C;
constexpr std::size_t VffHeaderSize = 0x20;

// A cluster has 0x200 bytes. The header counts them in units of 16 bytes.
constexpr std::size_t VolumeClusterSize = 0x200;
constexpr std::size_t ClusterSizeUnit = 16;
constexpr std::uint16_t ClusterSizeField = VolumeClusterSize / ClusterSizeUnit;

constexpr std::size_t FatCopyCount = 2;
constexpr std::size_t RootDirectorySize = 0x1000;
constexpr std::size_t DirectoryEntrySize = 0x20;

// A FAT with fewer clusters than this has entries of 12 bits, one with more clusters has entries of 16 bits. That is the rule of
// every FAT file system. The room for the FAT is worked out as if all entries had 16 bits, also for the small mailbox of 12 bits.
constexpr std::size_t FirstClusterCountWithSixteenBitEntries = 4085;
constexpr std::size_t SixteenBitEntrySize = 2;

// The first entries of the FAT: the media byte, the end mark, and the end mark of the only cluster of the directory /mb.
struct FatMarks
{
    std::uint16_t media;
    std::uint16_t endOfChain;
};

constexpr FatMarks TwelveBitMarks = {0xFF0, 0xFFF};
constexpr FatMarks SixteenBitMarks = {0xFFF0, 0xFFFF};
constexpr std::size_t MediaEntryNumber = 0;
constexpr std::size_t FatEndMarkEntryNumber = 1;
constexpr std::uint16_t DirectoryCluster = 2;
constexpr std::uint16_t RootDirectoryCluster = 0;

// Directory entries

constexpr std::uint8_t DirectoryAttribute = 0x10;
constexpr std::uint8_t LongNameAttribute = 0x0F;
constexpr std::string_view DirectoryName = "mb";
constexpr std::string_view ShortDirectoryName = "MB         ";
constexpr std::string_view DotName = ".          ";
constexpr std::string_view DotDotName = "..         ";
constexpr std::size_t ShortNameLength = 11;

// The place of the fields inside a short directory entry.
constexpr std::size_t AttributeOffset = 11;
constexpr std::size_t CreationTenthsOffset = 13;
constexpr std::size_t CreationTimeOffset = 14;
constexpr std::size_t CreationDateOffset = 16;
constexpr std::size_t AccessDateOffset = 18;
constexpr std::size_t WriteTimeOffset = 22;
constexpr std::size_t WriteDateOffset = 24;
constexpr std::size_t FirstClusterOffset = 26;

// The place of the fields inside an entry that holds a part of a long name.
constexpr std::uint8_t LastLongNameEntryMark = 0x40;
constexpr std::size_t LongNameTypeOffset = 12;
constexpr std::size_t LongNameChecksumOffset = 13;
constexpr std::size_t LongNameFirstCharactersOffset = 1;
constexpr std::size_t LongNameFirstCharacterCount = 5;
constexpr std::size_t BytesPerCharacter = 2;
constexpr std::uint16_t UnusedCharacter = 0xFFFF;
constexpr Formats::Byte UnusedByte = 0xFF;

void PutLittleEndianUint16(Formats::Bytes& bytes, std::size_t offset, std::uint16_t value)
{
    PutByte(bytes, offset, static_cast<Formats::Byte>(value));
    PutByte(bytes, offset + 1, static_cast<Formats::Byte>(value >> Formats::BitsPerByte));
}

// An entry of 12 bits shares its bytes with the next one: the first one of a pair takes a byte and the low half of the next byte,
// the second one takes the high half of that byte and the byte after it. The other half of a shared byte is left as it is.
void PutTwelveBitFatEntry(Formats::Bytes& bytes, std::size_t fatOffset, std::size_t entryNumber, std::uint16_t value)
{
    constexpr unsigned int lowHalfMask = 0x0F;
    constexpr unsigned int highHalfMask = 0xF0;
    constexpr int bitsPerHalfByte = 4;

    std::size_t byteOffset = fatOffset + entryNumber * 3 / 2;
    bool isFirstOfThePair = entryNumber % 2 == 0;

    if (isFirstOfThePair)
    {
        unsigned int highBits = (value >> Formats::BitsPerByte) & lowHalfMask;
        unsigned int keptHalf = bytes.at(byteOffset + 1) & highHalfMask;
        PutByte(bytes, byteOffset, static_cast<Formats::Byte>(value));
        PutByte(bytes, byteOffset + 1, static_cast<Formats::Byte>(keptHalf | highBits));
        return;
    }

    unsigned int lowBits = (value & lowHalfMask) << bitsPerHalfByte;
    unsigned int keptHalf = bytes.at(byteOffset) & lowHalfMask;
    PutByte(bytes, byteOffset, static_cast<Formats::Byte>(keptHalf | lowBits));
    PutByte(bytes, byteOffset + 1, static_cast<Formats::Byte>(value >> bitsPerHalfByte));
}

void PutFatEntry(Formats::Bytes& bytes, std::size_t fatOffset, std::size_t entryNumber, std::uint16_t value, bool hasTwelveBitEntries)
{
    if (hasTwelveBitEntries)
        PutTwelveBitFatEntry(bytes, fatOffset, entryNumber, value);
    else
        PutLittleEndianUint16(bytes, fatOffset + entryNumber * SixteenBitEntrySize, value);
}

void PutFirstFatEntries(Formats::Bytes& bytes, std::size_t fatOffset, bool hasTwelveBitEntries)
{
    FatMarks marks = hasTwelveBitEntries ? TwelveBitMarks : SixteenBitMarks;

    PutFatEntry(bytes, fatOffset, MediaEntryNumber, marks.media, hasTwelveBitEntries);
    PutFatEntry(bytes, fatOffset, FatEndMarkEntryNumber, marks.endOfChain, hasTwelveBitEntries);
    PutFatEntry(bytes, fatOffset, DirectoryCluster, marks.endOfChain, hasTwelveBitEntries);
}

// The checksum that ties the entries of a long name to the entry of the short name.
std::uint8_t ChecksumOfShortName(std::string_view shortName)
{
    std::uint8_t checksum = 0;
    for (char character : shortName)
    {
        constexpr std::uint8_t highestBit = 0x80;
        std::uint8_t carry = (checksum & 1) ? highestBit : 0;
        checksum = static_cast<std::uint8_t>(carry + (checksum >> 1) + static_cast<std::uint8_t>(character));
    }
    return checksum;
}

void PutShortEntry(Formats::Bytes& bytes, std::size_t offset, std::string_view shortName, std::uint16_t firstCluster, const FatTimestamp& timestamp)
{
    PutText(bytes, offset, shortName, ShortNameLength);
    PutByte(bytes, offset + AttributeOffset, DirectoryAttribute);
    PutByte(bytes, offset + CreationTenthsOffset, timestamp.hundredthsOfSecond);
    PutLittleEndianUint16(bytes, offset + CreationTimeOffset, timestamp.time);
    PutLittleEndianUint16(bytes, offset + CreationDateOffset, timestamp.date);
    PutLittleEndianUint16(bytes, offset + AccessDateOffset, timestamp.date);
    PutLittleEndianUint16(bytes, offset + WriteTimeOffset, timestamp.time);
    PutLittleEndianUint16(bytes, offset + WriteDateOffset, timestamp.date);
    PutLittleEndianUint16(bytes, offset + FirstClusterOffset, firstCluster);
}

// A name of up to five characters fits one entry. The characters are UTF-16, the end of the name is a zero character,
// and all unused places hold 0xFFFF.
void PutLongNameEntry(Formats::Bytes& bytes, std::size_t offset, std::string_view name, std::uint8_t checksum)
{
    constexpr std::uint8_t ordinalOfTheOnlyEntry = 1;

    for (std::size_t position = 1; position < DirectoryEntrySize; ++position)
        PutByte(bytes, offset + position, UnusedByte);

    PutByte(bytes, offset, LastLongNameEntryMark | ordinalOfTheOnlyEntry);
    for (std::size_t index = 0; index < LongNameFirstCharacterCount; ++index)
    {
        std::uint16_t character = UnusedCharacter;
        if (index < name.size())
            character = static_cast<std::uint8_t>(name[index]);
        else if (index == name.size())
            character = 0;

        PutLittleEndianUint16(bytes, offset + LongNameFirstCharactersOffset + index * BytesPerCharacter, character);
    }

    PutByte(bytes, offset + AttributeOffset, LongNameAttribute);
    PutByte(bytes, offset + LongNameTypeOffset, 0);
    PutByte(bytes, offset + LongNameChecksumOffset, checksum);
    PutLittleEndianUint16(bytes, offset + FirstClusterOffset, 0);
}

struct VolumeLayout
{
    std::size_t fatSize;
    std::size_t rootDirectoryOffset;
    std::size_t firstClusterOffset;
    bool hasTwelveBitEntries;
};

// The FAT has an entry for each cluster and two more. Its size is a whole number of clusters, and as small as it can be.
VolumeLayout LayOut(std::size_t volumeSize)
{
    std::size_t fatSize = VolumeClusterSize;
    while (true)
    {
        std::size_t rootDirectoryOffset = VffHeaderSize + FatCopyCount * fatSize;
        std::size_t firstClusterOffset = rootDirectoryOffset + RootDirectorySize;
        std::size_t clusterCount = (volumeSize - firstClusterOffset) / VolumeClusterSize;
        bool fatIsBigEnough = (clusterCount + 2) * SixteenBitEntrySize <= fatSize;
        if (fatIsBigEnough)
        {
            bool hasTwelveBitEntries = clusterCount < FirstClusterCountWithSixteenBitEntries;
            return VolumeLayout{fatSize, rootDirectoryOffset, firstClusterOffset, hasTwelveBitEntries};
        }

        fatSize += VolumeClusterSize;
    }
}

}

FatTimestamp MakeFatTimestamp(std::chrono::system_clock::time_point moment)
{
    constexpr int firstYear = 1980;
    constexpr int yearShift = 9;
    constexpr int monthShift = 5;
    constexpr int hourShift = 11;
    constexpr int minuteShift = 5;

    std::chrono::sys_days day = std::chrono::floor<std::chrono::days>(moment);
    std::chrono::year_month_day date(day);
    std::chrono::hh_mm_ss<std::chrono::milliseconds> timeOfDay(std::chrono::floor<std::chrono::milliseconds>(moment - day));

    if (static_cast<int>(date.year()) < firstYear)
        return MakeFatTimestamp(std::chrono::sys_days(std::chrono::year(firstYear) / std::chrono::January / 1));

    int years = static_cast<int>(date.year()) - firstYear;
    unsigned int month = static_cast<unsigned int>(date.month());
    unsigned int dayOfMonth = static_cast<unsigned int>(date.day());
    int seconds = static_cast<int>(timeOfDay.seconds().count());
    int milliseconds = static_cast<int>(timeOfDay.subseconds().count());

    FatTimestamp timestamp;
    timestamp.date = static_cast<std::uint16_t>((years << yearShift) | (month << monthShift) | dayOfMonth);
    timestamp.time = static_cast<std::uint16_t>((timeOfDay.hours().count() << hourShift) | (timeOfDay.minutes().count() << minuteShift) | (seconds / 2));
    timestamp.hundredthsOfSecond = static_cast<std::uint8_t>((seconds % 2) * 100 + milliseconds / 10);
    return timestamp;
}

Formats::Bytes MakeMailboxVolume(Mailbox mailbox, const FatTimestamp& timestamp)
{
    std::uint32_t volumeSize = MailboxVolumeSize(mailbox);
    VolumeLayout layout = LayOut(volumeSize);
    Formats::Bytes bytes(volumeSize);

    PutUint32(bytes, 0, VffMagic);
    PutUint16(bytes, ByteOrderMarkOffset, ByteOrderMark);
    PutUint16(bytes, VersionOffset, VffVersion);
    PutUint32(bytes, VolumeSizeOffset, volumeSize);
    PutUint16(bytes, ClusterSizeFieldOffset, ClusterSizeField);

    for (std::size_t copy = 0; copy < FatCopyCount; ++copy)
        PutFirstFatEntries(bytes, VffHeaderSize + copy * layout.fatSize, layout.hasTwelveBitEntries);

    std::uint8_t checksum = ChecksumOfShortName(ShortDirectoryName);
    PutLongNameEntry(bytes, layout.rootDirectoryOffset, DirectoryName, checksum);
    PutShortEntry(bytes, layout.rootDirectoryOffset + DirectoryEntrySize, ShortDirectoryName, DirectoryCluster, timestamp);

    PutShortEntry(bytes, layout.firstClusterOffset, DotName, DirectoryCluster, timestamp);
    PutShortEntry(bytes, layout.firstClusterOffset + DirectoryEntrySize, DotDotName, RootDirectoryCluster, timestamp);
    return bytes;
}

bool IsMailboxVolume(Formats::ByteView firstBytes, std::uint64_t fileLength, Mailbox mailbox)
{
    constexpr std::size_t bytesToLookAt = VolumeSizeOffset + sizeof(std::uint32_t);

    if (fileLength != MailboxVolumeSize(mailbox) || firstBytes.size() < bytesToLookAt)
        return false;

    Formats::ByteReader reader(firstBytes, "Mailbox volume");
    if (reader.ReadUint32() != VffMagic)
        return false;

    reader.MoveTo(VolumeSizeOffset);
    return reader.ReadUint32() == fileLength;
}

}
