#pragma once

#include "SystemMenu/MessageBoard/BoardDays.h"
#include "SystemMenu/NandFiles.h"

#include "Formats/BoardMessageFile.h"
#include "Formats/CdbRecordFile.h"
#include "Formats/VirtualFatVolume.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

constexpr std::string_view BoardVolumePath = "/title/00000001/00000002/data/cdb.vff";
constexpr std::string_view BoardConsoleIdPath = "/title/00000001/00000002/data/nocopy/cdbwiiid.dat";

// The database makes cdb.vff with this size when it is missing (0x8148ACC8).
constexpr std::uint32_t BoardVolumeSize = 0x1400000;

// The folder of the records that the menu makes itself: the code of the Message Board title, HAEA, and its maker number 1,
// which the database writes as "#1" because it is not a letter or a digit (0x8149036C).
constexpr std::string_view BoardSourceFolder = "HAEA_#1";

// The kinds of records, as the folders below the source folder name them.
namespace KindFolder
{
constexpr std::string_view Text = "txt";
constexpr std::string_view Picture = "odh";
constexpr std::string_view Data = "dat";
constexpr std::string_view PlayLog = "log";
}

constexpr std::size_t WirelessAddressSize = 6;
using WirelessAddress = std::array<Formats::Byte, WirelessAddressSize>;

// Where a record lies in the volume: "<year>/<month>/<day>/<hour>/<minute>/<source>/<kind>/<seconds in hexadecimal>.<number>",
// for example "2023/10/12/00/06/HAEA_01/txt/2CE2D3A2.000" for 12 November 2023, 00:06:58: the months count from 00.
struct RecordPlace
{
    // Seconds since the start of 2000, local time: the day and the time at which the message lies on the board.
    std::uint32_t time = 0;
    std::string sourceFolder;
    std::string kindFolder;

    // 0 for the first record of a second, then 1, 2 and so on.
    int number = 0;

    bool operator==(const RecordPlace&) const = default;
};

std::string PathOf(const RecordPlace& place);

// Nothing for a path that is not the path of a record.
std::optional<RecordPlace> PlaceOfPath(std::string_view path);

// The key by which the database names a record: "<seconds, 10 digits>_<source>_<number, 3 digits>.<kind>", for example
// "0752022178_HAEA_#1_000.txt" (0x8148E378).
std::string RecordKeyOf(const RecordPlace& place);

struct StoredMessage
{
    RecordPlace place;
    Formats::CdbRecordFile record;
    Formats::BoardMessage message;
};

// The Message Board's database (the CDB library 0x81485FB8 to 0x81494000 and the CdbManager 0x8133F068 to 0x81340584) in cdb.vff on
// the NAND. The volume is read once and written back after every change. A volume that cannot be read counts as empty and is made
// anew at the next change, as the original repairs a damaged database.
class MessageStore
{
public:
    // The address is the console's wireless address: the console id of the records is made from it when the NAND has no
    // cdbwiiid.dat. The files must outlive the store.
    MessageStore(NandFiles& nandFiles, const WirelessAddress& wirelessAddress);

    // The messages that lie on a day, the newest first. Records that cannot be read are left out, as the board leaves them out.
    // Writes an empty database when cdb.vff is missing, as the menu does when it starts (the CdbManager's set-up, stage 3): the file
    // lies in the menu's own data folder, where the channels that post to the board cannot make it.
    void CreateWhenMissing();

    std::vector<StoredMessage> MessagesOfDay(BoardDay day);

    // The days whose folders hold records, the oldest first: every day a program that looks through the whole board reads.
    std::vector<BoardDay> DaysWithRecords();

    // The records of a day that the bar of the Wii Menu counts: letters, memos and play logs (0x81391904).
    int CountMessagesOfDay(BoardDay day);

    // True when a day has a letter or a memo; play logs do not count. The calendar marks these days (0x8139F51C).
    bool HasLettersOnDay(BoardDay day);

    // Puts a new record at this time. "now" is the time of the console, for the counter of writes and the time of the change.
    // Throws a MenuError when the NAND does not take the volume, or the volume is full.
    StoredMessage AddMessage(std::string_view typeName, std::string_view kindFolder, const LocalTime& time,
        const Formats::BoardMessage& message, const LocalTime& now);

    // Writes the changed message of a record back, one more write.
    void ChangeMessage(StoredMessage& stored, const LocalTime& now);

    // Deletes the record and the folders that it leaves empty.
    void DeleteMessage(const RecordPlace& place);

    // The same, but cdb.vff on the NAND changes only at the next SaveToNand: for a board that deletes many records one after the other.
    void DeleteMessageWithoutSaving(const RecordPlace& place);
    void SaveToNand();

    // Every record of every kind and source, the oldest first, as the database goes through them from 2000 on (0x8148967C).
    std::vector<RecordPlace> RecordsOldestFirst();

    // The whole file of a record. Nothing when it is not there.
    std::optional<Formats::Bytes> ReadRecordFile(const RecordPlace& place);

    std::uint64_t FreeBytes();

    // The first eight bytes of cdbwiiid.dat: the SHA-1 of the wireless address and "uyy" (0x8148FD3C).
    std::array<Formats::Byte, Formats::CdbConsoleIdSize> ConsoleId();

    // The key of the HMAC of the copies on the SD card: the 12 bytes of cdbwiiid.dat after the console id (0x8149010C).
    Formats::Bytes SdCopyKey();

private:
    Formats::VirtualFatVolume& Volume();
    Formats::Bytes ConsoleIdFile();

    std::vector<std::string> RecordPathsOfDay(BoardDay day);

    std::uint32_t TakeRecordNumber(const LocalTime& now);

    NandFiles& nand;
    WirelessAddress address;
    std::optional<Formats::VirtualFatVolume> volume;
};

}
