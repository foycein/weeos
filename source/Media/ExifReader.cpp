#include "Media/ExifReader.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <string>
#include <string_view>

namespace Media
{

namespace
{

constexpr std::string_view DataDescription = "Exif segment";

constexpr Formats::Byte MarkerPrefix = 0xFF;
constexpr Formats::Byte StartOfImage = 0xD8;
constexpr Formats::Byte ExifSegmentMarker = 0xE1;
constexpr Formats::Byte StartOfScan = 0xDA;
constexpr std::size_t SegmentLengthSize = 2;
constexpr std::string_view ExifSignature = std::string_view("Exif\0\0", 6);

// The Exif data is a small TIFF file: its byte order ("II" least significant byte first, "MM" most significant first), the number
// 42, and the place of the first image directory. Every place in it counts from the start of the TIFF header.
constexpr std::string_view LeastSignificantFirst = "II";
constexpr std::string_view MostSignificantFirst = "MM";
constexpr std::uint16_t TiffNumber = 42;

constexpr std::uint16_t OrientationTag = 0x0112;
constexpr std::uint16_t DateTimeTag = 0x0132;
constexpr std::uint16_t ExifDirectoryTag = 0x8769;
constexpr std::uint16_t DateTimeOriginalTag = 0x9003;
constexpr std::uint16_t ThumbnailPlaceTag = 0x0201;
constexpr std::uint16_t ThumbnailLengthTag = 0x0202;

// The text of an Exif date is "YYYY:MM:DD HH:MM:SS"; the places of its fields.
constexpr std::size_t DateTextSize = 19;
constexpr std::size_t YearPlace = 0;
constexpr std::size_t MonthPlace = 5;
constexpr std::size_t DayPlace = 8;
constexpr std::size_t HourPlace = 11;
constexpr std::size_t MinutePlace = 14;
constexpr std::size_t SecondPlace = 17;
constexpr std::size_t YearDigits = 4;
constexpr std::size_t OtherFieldDigits = 2;
constexpr int DecimalBase = 10;
constexpr int FirstYearOfADate = 1900;
constexpr int LastYearOfADate = 2099;

// Reads the numbers of the TIFF data in its byte order.
class TiffData
{
public:
    explicit TiffData(Formats::ByteView tiffBytes)
        : bytes(tiffBytes)
    {
        Formats::ByteReader reader(bytes, DataDescription);
        std::string byteOrder = reader.ReadText(LeastSignificantFirst.size());
        if (byteOrder == LeastSignificantFirst)
            isLeastSignificantFirst = true;
        else if (byteOrder != MostSignificantFirst)
            reader.Fail("the TIFF header names no byte order");

        if (ReadUint16(reader) != TiffNumber)
            reader.Fail("the TIFF header lacks the number 42");

        firstDirectoryPlace = ReadUint32(reader);
    }

    std::uint16_t ReadUint16(Formats::ByteReader& reader) const
    {
        return isLeastSignificantFirst ? reader.ReadLittleEndianUint16() : reader.ReadUint16();
    }

    std::uint32_t ReadUint32(Formats::ByteReader& reader) const
    {
        return isLeastSignificantFirst ? reader.ReadLittleEndianUint32() : reader.ReadUint32();
    }

    Formats::ByteView bytes;
    bool isLeastSignificantFirst = false;
    std::uint32_t firstDirectoryPlace = 0;
};

// An entry of a directory: tag, type, count, and the value itself when it fits in four bytes, else the place of the value.
struct DirectoryEntry
{
    std::uint16_t tag = 0;
    std::uint32_t count = 0;

    // The first two bytes as a number, and all four as a number or as the place of the value.
    std::uint16_t shortValue = 0;
    std::uint32_t longValueOrPlace = 0;
};

struct Directory
{
    std::vector<DirectoryEntry> entries;
    std::uint32_t nextDirectoryPlace = 0;

    const DirectoryEntry* Find(std::uint16_t tag) const
    {
        for (const DirectoryEntry& entry : entries)
        {
            if (entry.tag == tag)
                return &entry;
        }
        return nullptr;
    }
};

Directory ReadDirectory(const TiffData& tiff, std::uint32_t place)
{
    Formats::ByteReader reader(tiff.bytes, DataDescription);
    reader.MoveTo(place);

    Directory directory;
    std::uint16_t entryCount = tiff.ReadUint16(reader);
    for (std::uint16_t entryNumber = 0; entryNumber < entryCount; ++entryNumber)
    {
        DirectoryEntry entry;
        entry.tag = tiff.ReadUint16(reader);
        reader.Skip(sizeof(std::uint16_t));
        entry.count = tiff.ReadUint32(reader);

        std::size_t valuePlace = reader.Position();
        entry.shortValue = tiff.ReadUint16(reader);
        reader.MoveTo(valuePlace);
        entry.longValueOrPlace = tiff.ReadUint32(reader);

        directory.entries.push_back(entry);
    }

    directory.nextDirectoryPlace = tiff.ReadUint32(reader);
    return directory;
}

// A number of count digits at start, or nothing when one of them is not a digit.
std::optional<int> ReadDigits(std::string_view text, std::size_t start, std::size_t count)
{
    int number = 0;
    for (std::size_t place = start; place < start + count; ++place)
    {
        char character = text[place];
        if (character < '0' || character > '9')
            return std::nullopt;

        number = number * DecimalBase + (character - '0');
    }
    return number;
}

std::optional<ExifDateTime> ParseDateText(std::string_view text)
{
    std::optional<int> year = ReadDigits(text, YearPlace, YearDigits);
    std::optional<int> month = ReadDigits(text, MonthPlace, OtherFieldDigits);
    std::optional<int> day = ReadDigits(text, DayPlace, OtherFieldDigits);
    std::optional<int> hour = ReadDigits(text, HourPlace, OtherFieldDigits);
    std::optional<int> minute = ReadDigits(text, MinutePlace, OtherFieldDigits);
    std::optional<int> second = ReadDigits(text, SecondPlace, OtherFieldDigits);
    if (!year || !month || !day || !hour || !minute || !second)
        return std::nullopt;

    return ExifDateTime{*year, *month, *day, *hour, *minute, *second};
}

std::optional<ExifDateTime> ReadDate(const TiffData& tiff, const DirectoryEntry* entry)
{
    if (entry == nullptr || entry->count < DateTextSize)
        return std::nullopt;

    if (!Formats::IsRangeInside(entry->longValueOrPlace, DateTextSize, tiff.bytes.size()))
        return std::nullopt;

    Formats::ByteView textBytes = tiff.bytes.subspan(entry->longValueOrPlace, DateTextSize);
    std::string text(textBytes.begin(), textBytes.end());

    std::optional<ExifDateTime> date = ParseDateText(text);
    if (!date)
        return std::nullopt;

    if (date->year < FirstYearOfADate || date->year > LastYearOfADate)
        return std::nullopt;

    return date;
}

std::optional<Formats::ByteView> ReadThumbnail(const TiffData& tiff, const Directory& secondDirectory)
{
    const DirectoryEntry* place = secondDirectory.Find(ThumbnailPlaceTag);
    const DirectoryEntry* length = secondDirectory.Find(ThumbnailLengthTag);
    if (place == nullptr || length == nullptr)
        return std::nullopt;

    if (!Formats::IsRangeInside(place->longValueOrPlace, length->longValueOrPlace, tiff.bytes.size()))
        return std::nullopt;

    return tiff.bytes.subspan(place->longValueOrPlace, length->longValueOrPlace);
}

ExifInformation ReadTiffData(Formats::ByteView tiffBytes)
{
    TiffData tiff(tiffBytes);
    Directory firstDirectory = ReadDirectory(tiff, tiff.firstDirectoryPlace);

    ExifInformation information;
    const DirectoryEntry* orientation = firstDirectory.Find(OrientationTag);
    if (orientation != nullptr)
        information.orientation = orientation->shortValue;

    const DirectoryEntry* exifDirectoryPlace = firstDirectory.Find(ExifDirectoryTag);
    if (exifDirectoryPlace != nullptr)
    {
        Directory exifDirectory = ReadDirectory(tiff, exifDirectoryPlace->longValueOrPlace);
        information.dateTaken = ReadDate(tiff, exifDirectory.Find(DateTimeOriginalTag));
    }

    if (!information.dateTaken)
        information.dateTaken = ReadDate(tiff, firstDirectory.Find(DateTimeTag));

    if (firstDirectory.nextDirectoryPlace == 0)
        return information;

    try
    {
        information.thumbnail = ReadThumbnail(tiff, ReadDirectory(tiff, firstDirectory.nextDirectoryPlace));
    }
    catch (const Formats::FormatError&)
    {
        information.thumbnail = std::nullopt;
    }
    return information;
}

// The segments before the first scan, looking for APP1 with the signature "Exif".
std::optional<Formats::ByteView> FindExifTiffData(Formats::ByteView jpegBytes)
{
    Formats::ByteReader reader(jpegBytes, DataDescription);
    if (reader.ReadByte() != MarkerPrefix || reader.ReadByte() != StartOfImage)
        return std::nullopt;

    while (reader.RemainingSize() > SegmentLengthSize)
    {
        if (reader.ReadByte() != MarkerPrefix)
            return std::nullopt;

        Formats::Byte marker = reader.ReadByte();
        if (marker == StartOfScan)
            return std::nullopt;

        std::uint16_t segmentLength = reader.ReadUint16();
        if (segmentLength < SegmentLengthSize)
            return std::nullopt;

        Formats::ByteView segment = reader.ReadBytes(segmentLength - SegmentLengthSize);
        bool hasSignature = segment.size() > ExifSignature.size()
            && std::string_view(reinterpret_cast<const char*>(segment.data()), ExifSignature.size()) == ExifSignature;
        if (marker == ExifSegmentMarker && hasSignature)
            return segment.subspan(ExifSignature.size());
    }
    return std::nullopt;
}

}

ExifInformation ReadExifInformation(Formats::ByteView jpegBytes)
{
    try
    {
        std::optional<Formats::ByteView> tiffData = FindExifTiffData(jpegBytes);
        if (!tiffData)
            return {};

        return ReadTiffData(*tiffData);
    }
    catch (const Formats::FormatError&)
    {
        return {};
    }
}

}
