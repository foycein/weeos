#include "SystemMenu/MessageBoard/MessageStore.h"

#include "SystemMenu/MenuError.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"
#include "Formats/Sha1.h"

#include <algorithm>
#include <charconv>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view CounterFileName = "cdb.conf";

// The text that the database adds to the wireless address before it takes the SHA-1 (0x8148FD3C).
constexpr std::string_view ConsoleIdSalt = "uyy";
constexpr std::size_t ConsoleIdFileSize = 0x28;
constexpr std::size_t SdCopyKeyStart = 0x08;
constexpr std::size_t SdCopyKeyEnd = 0x14;

constexpr std::array<std::string_view, 3> LetterKindFolders = {KindFolder::Text, KindFolder::Picture, KindFolder::Data};

// The parts of the path of a record.
constexpr std::size_t PathPartCount = 8;
constexpr std::size_t SourcePart = 5;
constexpr std::size_t KindPart = 6;
constexpr std::size_t TimeDigitCount = 8;
constexpr std::size_t NumberDigitCount = 3;
constexpr int HexadecimalBase = 16;

Formats::VolumeTime VolumeTimeOf(const LocalTime& time)
{
    return Formats::VolumeTime{time.year, time.month, time.day, time.hour, time.minute, time.second};
}

// The folder of a month counts from 00 for January, as the calendar time of the operating system does; the days count from 01.
int MonthFolderNumber(int month)
{
    return month - 1;
}

std::string DayFolderOf(BoardDay day)
{
    return std::format("{:04}/{:02}/{:02}", day.year, MonthFolderNumber(day.month), day.day);
}

std::vector<std::string_view> SplitPath(std::string_view path)
{
    std::vector<std::string_view> parts;
    while (true)
    {
        std::size_t slash = path.find('/');
        parts.push_back(path.substr(0, slash));
        if (slash == std::string_view::npos)
            return parts;
        path.remove_prefix(slash + 1);
    }
}

std::optional<std::uint32_t> ReadHexadecimal(std::string_view text)
{
    std::uint32_t value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, HexadecimalBase);
    if (error != std::errc() || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

std::optional<int> ReadDecimal(std::string_view text)
{
    int value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

// The number that a folder of a year, a month or a day is named with; nothing for another entry.
std::optional<int> NumberOfFolder(const Formats::VolumeEntry& entry)
{
    if (!entry.isFolder)
        return std::nullopt;

    return ReadDecimal(entry.name);
}

bool IsLetterKind(std::string_view kindFolder)
{
    return std::find(LetterKindFolders.begin(), LetterKindFolders.end(), kindFolder) != LetterKindFolders.end();
}

bool IsBoardKind(std::string_view kindFolder)
{
    return IsLetterKind(kindFolder) || kindFolder == KindFolder::PlayLog;
}

}

std::string PathOf(const RecordPlace& place)
{
    LocalTime time = TimeOfSeconds(place.time);
    return std::format("{}/{:02}/{:02}/{}/{}/{:08X}.{:03}", DayFolderOf(DayOf(time)), time.hour, time.minute, place.sourceFolder,
        place.kindFolder, place.time, place.number);
}

std::optional<RecordPlace> PlaceOfPath(std::string_view path)
{
    std::vector<std::string_view> parts = SplitPath(path);
    if (parts.size() != PathPartCount)
        return std::nullopt;

    std::string_view fileName = parts.back();
    std::size_t dot = fileName.find('.');
    if (dot != TimeDigitCount || fileName.size() != TimeDigitCount + 1 + NumberDigitCount)
        return std::nullopt;

    std::optional<std::uint32_t> time = ReadHexadecimal(fileName.substr(0, dot));
    std::optional<int> number = ReadDecimal(fileName.substr(dot + 1));
    if (!time || !number)
        return std::nullopt;

    RecordPlace place;
    place.time = *time;
    place.sourceFolder = std::string(parts[SourcePart]);
    place.kindFolder = std::string(parts[KindPart]);
    place.number = *number;
    return place;
}

std::string RecordKeyOf(const RecordPlace& place)
{
    return std::format("{:010}_{}_{:03}.{}", place.time, place.sourceFolder, place.number, place.kindFolder);
}

MessageStore::MessageStore(NandFiles& nandFiles, const WirelessAddress& wirelessAddress)
    : nand(nandFiles)
    , address(wirelessAddress)
{
}

Formats::VirtualFatVolume& MessageStore::Volume()
{
    if (volume)
        return *volume;

    std::optional<Formats::Bytes> fileBytes = nand.ReadFile(BoardVolumePath);
    if (fileBytes)
    {
        try
        {
            volume = Formats::VirtualFatVolume::Read(std::move(*fileBytes));
            return *volume;
        }
        catch (const Formats::FormatError&)
        {
            volume.reset();
        }
    }

    volume = Formats::VirtualFatVolume::MakeEmpty(BoardVolumeSize);
    return *volume;
}

void MessageStore::SaveToNand()
{
    nand.WriteFile(BoardVolumePath, Volume().FileBytes());
}

void MessageStore::CreateWhenMissing()
{
    if (nand.ReadFile(BoardVolumePath))
        return;

    SaveToNand();
}

std::vector<std::string> MessageStore::RecordPathsOfDay(BoardDay day)
{
    Formats::VirtualFatVolume& board = Volume();
    std::string dayFolder = DayFolderOf(day);
    std::vector<std::string> paths;
    for (const Formats::VolumeEntry& hour : board.ListFolder(dayFolder))
    {
        std::string hourFolder = dayFolder + "/" + hour.name;
        for (const Formats::VolumeEntry& minute : board.ListFolder(hourFolder))
        {
            std::string minuteFolder = hourFolder + "/" + minute.name;
            for (const Formats::VolumeEntry& source : board.ListFolder(minuteFolder))
            {
                std::string sourceFolder = minuteFolder + "/" + source.name;
                for (const Formats::VolumeEntry& kind : board.ListFolder(sourceFolder))
                {
                    std::string kindFolder = sourceFolder + "/" + kind.name;
                    for (const Formats::VolumeEntry& file : board.ListFolder(kindFolder))
                    {
                        if (!file.isFolder)
                            paths.push_back(kindFolder + "/" + file.name);
                    }
                }
            }
        }
    }
    return paths;
}

// The folders "<year>/<month from 00>/<day>" at the top of the volume; names that are not numbers are passed over.
std::vector<BoardDay> MessageStore::DaysWithRecords()
{
    Formats::VirtualFatVolume& board = Volume();
    std::vector<BoardDay> days;
    for (const Formats::VolumeEntry& year : board.ListFolder(""))
    {
        std::optional<int> yearNumber = NumberOfFolder(year);
        if (!yearNumber)
            continue;

        for (const Formats::VolumeEntry& month : board.ListFolder(year.name))
        {
            std::optional<int> monthNumber = NumberOfFolder(month);
            if (!monthNumber)
                continue;

            for (const Formats::VolumeEntry& day : board.ListFolder(year.name + "/" + month.name))
            {
                std::optional<int> dayNumber = NumberOfFolder(day);
                if (dayNumber)
                    days.push_back(BoardDay{*yearNumber, *monthNumber + 1, *dayNumber});
            }
        }
    }
    std::sort(days.begin(), days.end());
    return days;
}

std::vector<StoredMessage> MessageStore::MessagesOfDay(BoardDay day)
{
    std::vector<StoredMessage> messages;
    for (const std::string& path : RecordPathsOfDay(day))
    {
        std::optional<RecordPlace> place = PlaceOfPath(path);
        if (!place || !IsBoardKind(place->kindFolder))
            continue;

        try
        {
            StoredMessage stored;
            stored.place = *place;
            stored.record = Formats::ReadCdbRecordFile(Volume().ReadFile(path).value());
            stored.message = Formats::ReadBoardMessage(stored.record.data);
            messages.push_back(std::move(stored));
        }
        catch (const Formats::FormatError&)
        {
        }
    }

    std::sort(messages.begin(), messages.end(), [](const StoredMessage& first, const StoredMessage& second) {
        if (first.place.time != second.place.time)
            return first.place.time > second.place.time;
        return first.place.number > second.place.number;
    });
    return messages;
}

int MessageStore::CountMessagesOfDay(BoardDay day)
{
    int count = 0;
    for (const std::string& path : RecordPathsOfDay(day))
    {
        std::optional<RecordPlace> place = PlaceOfPath(path);
        if (place && IsBoardKind(place->kindFolder))
            ++count;
    }
    return count;
}

bool MessageStore::HasLettersOnDay(BoardDay day)
{
    for (const std::string& path : RecordPathsOfDay(day))
    {
        std::optional<RecordPlace> place = PlaceOfPath(path);
        if (place && IsLetterKind(place->kindFolder))
            return true;
    }
    return false;
}

StoredMessage MessageStore::AddMessage(std::string_view typeName, std::string_view kindFolder, const LocalTime& time,
    const Formats::BoardMessage& message, const LocalTime& now)
{
    StoredMessage stored;
    stored.place.time = SecondsSince2000(time);
    stored.place.sourceFolder = std::string(BoardSourceFolder);
    stored.place.kindFolder = std::string(kindFolder);
    stored.record.consoleId = ConsoleId();
    stored.record.typeName = std::string(typeName);
    stored.record.writeCount = 1;
    stored.record.modifiedTime = SecondsSince2000(now);
    stored.record.data = Formats::WriteBoardMessage(message);
    stored.message = message;

    try
    {
        while (Volume().ReadFile(PathOf(stored.place)))
            ++stored.place.number;

        stored.record.recordNumber = TakeRecordNumber(now);
        Volume().WriteFile(PathOf(stored.place), Formats::WriteCdbRecordFile(stored.record), VolumeTimeOf(now));
    }
    catch (const Formats::FormatError& error)
    {
        throw MenuError(std::format("The Message Board cannot take the message: {}", error.what()));
    }
    SaveToNand();
    return stored;
}

void MessageStore::ChangeMessage(StoredMessage& stored, const LocalTime& now)
{
    stored.record.data = Formats::WriteBoardMessage(stored.message);
    stored.record.writeCount += 1;
    stored.record.modifiedTime = SecondsSince2000(now);
    try
    {
        Volume().WriteFile(PathOf(stored.place), Formats::WriteCdbRecordFile(stored.record), VolumeTimeOf(now));
    }
    catch (const Formats::FormatError& error)
    {
        throw MenuError(std::format("The Message Board cannot take the changed message: {}", error.what()));
    }
    SaveToNand();
}

void MessageStore::DeleteMessage(const RecordPlace& place)
{
    DeleteMessageWithoutSaving(place);
    SaveToNand();
}

void MessageStore::DeleteMessageWithoutSaving(const RecordPlace& place)
{
    std::string path = PathOf(place);
    if (!Volume().DeleteFile(path))
        return;

    std::string folder = path.substr(0, path.rfind('/'));
    while (!folder.empty() && Volume().DeleteFolder(folder))
    {
        std::size_t slash = folder.rfind('/');
        folder = slash == std::string::npos ? std::string() : folder.substr(0, slash);
    }
}

std::vector<RecordPlace> MessageStore::RecordsOldestFirst()
{
    std::vector<RecordPlace> places;
    for (BoardDay day : DaysWithRecords())
    {
        for (const std::string& path : RecordPathsOfDay(day))
        {
            std::optional<RecordPlace> place = PlaceOfPath(path);
            if (place)
                places.push_back(*place);
        }
    }

    std::stable_sort(places.begin(), places.end(), [](const RecordPlace& first, const RecordPlace& second) {
        if (first.time != second.time)
            return first.time < second.time;
        return first.number < second.number;
    });
    return places;
}

std::optional<Formats::Bytes> MessageStore::ReadRecordFile(const RecordPlace& place)
{
    return Volume().ReadFile(PathOf(place));
}

std::uint64_t MessageStore::FreeBytes()
{
    return Volume().FreeBytes();
}

// cdbwiiid.dat, or what the database would write into it: the SHA-1 of the wireless address and "uyy".
Formats::Bytes MessageStore::ConsoleIdFile()
{
    std::optional<Formats::Bytes> stored = nand.ReadFile(BoardConsoleIdPath);
    if (stored && stored->size() == ConsoleIdFileSize)
        return *stored;

    Formats::Bytes addressAndSalt(address.begin(), address.end());
    addressAndSalt.insert(addressAndSalt.end(), ConsoleIdSalt.begin(), ConsoleIdSalt.end());
    Formats::Sha1Digest digest = Formats::ComputeSha1(addressAndSalt);
    return Formats::Bytes(digest.begin(), digest.end());
}

// The database reads the 0x28 bytes of the file and then sets the last 0x14 to zero, so only the bytes 8 to 0x13 remain of the key
// (0x8148FD3C); the HMAC fills a short key with zeros anyway.
Formats::Bytes MessageStore::SdCopyKey()
{
    Formats::Bytes idFile = ConsoleIdFile();
    return Formats::Bytes(idFile.begin() + SdCopyKeyStart, idFile.begin() + SdCopyKeyEnd);
}

std::array<Formats::Byte, Formats::CdbConsoleIdSize> MessageStore::ConsoleId()
{
    Formats::Bytes idFile = ConsoleIdFile();
    std::array<Formats::Byte, Formats::CdbConsoleIdSize> id;
    std::copy(idFile.begin(), idFile.begin() + id.size(), id.begin());
    return id;
}

// The counter file cdb.conf holds the number of the last record that was made. A database without it starts with 0 (0x814920A8).
std::uint32_t MessageStore::TakeRecordNumber(const LocalTime& now)
{
    std::uint32_t number = 0;
    std::optional<Formats::Bytes> counter = Volume().ReadFile(CounterFileName);
    if (counter && counter->size() == sizeof(std::uint32_t))
        number = Formats::ByteReader(*counter, "cdb.conf").ReadUint32() + 1;

    Formats::ByteWriter newCounter;
    newCounter.WriteUint32(number);
    Volume().WriteFile(CounterFileName, newCounter.GetBytes(), VolumeTimeOf(now));
    return number;
}

}
