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

constexpr std::array<std::string_view, 3> LetterKindFolders = {KindFolder::Text, KindFolder::Picture, KindFolder::Data};

// The parts of the path of a record.
constexpr std::size_t PathPartCount = 8;
constexpr std::size_t TimeDigitCount = 8;
constexpr std::size_t NumberDigitCount = 3;
constexpr int HexadecimalBase = 16;

Formats::VolumeTime VolumeTimeOf(const LocalTime& time)
{
    return Formats::VolumeTime{time.year, time.month, time.day, time.hour, time.minute, time.second};
}

// The folder of a month counts from 00 for January, as the calendar time of the operating system does; the days count from 01.
// The number that a folder of a year, a month or a day is named with; nothing for another folder.
std::optional<int> NumberOfFolder(const std::string& name)
{
    if (name.empty() || !std::all_of(name.begin(), name.end(), [](char character) { return character >= '0' && character <= '9'; }))
        return std::nullopt;

    return std::stoi(name);
}

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
    place.sourceFolder = std::string(parts[5]);
    place.kindFolder = std::string(parts[6]);
    place.number = *number;
    return place;
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

void MessageStore::Save()
{
    nand.WriteFile(BoardVolumePath, Volume().FileBytes());
}

std::vector<BoardDay> MessageStore::DaysWithRecords()
{
    constexpr std::string_view RootFolder = "";
    Formats::VirtualFatVolume& board = Volume();
    std::vector<BoardDay> days;
    for (const Formats::VolumeEntry& year : board.ListFolder(RootFolder))
    {
        for (const Formats::VolumeEntry& month : board.ListFolder(year.name))
        {
            for (const Formats::VolumeEntry& day : board.ListFolder(year.name + "/" + month.name))
            {
                std::optional<int> yearNumber = NumberOfFolder(year.name);
                std::optional<int> monthNumber = NumberOfFolder(month.name);
                std::optional<int> dayNumber = NumberOfFolder(day.name);
                if (yearNumber && monthNumber && dayNumber)
                    days.push_back({*yearNumber, *monthNumber + 1, *dayNumber});
            }
        }
    }
    std::sort(days.begin(), days.end());
    return days;
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
    Save();
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
    Save();
}

void MessageStore::DeleteMessage(const RecordPlace& place)
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
    Save();
}

std::uint64_t MessageStore::FreeBytes()
{
    return Volume().FreeBytes();
}

std::array<Formats::Byte, Formats::CdbConsoleIdSize> MessageStore::ConsoleId()
{
    Formats::Bytes idFile;
    std::optional<Formats::Bytes> stored = nand.ReadFile(BoardConsoleIdPath);
    if (stored && stored->size() == ConsoleIdFileSize)
    {
        idFile = *stored;
    }
    else
    {
        Formats::Bytes addressAndSalt(address.begin(), address.end());
        addressAndSalt.insert(addressAndSalt.end(), ConsoleIdSalt.begin(), ConsoleIdSalt.end());
        Formats::Sha1Digest digest = Formats::ComputeSha1(addressAndSalt);
        idFile.assign(digest.begin(), digest.end());
    }

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
