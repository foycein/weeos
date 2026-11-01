#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Formats
{

constexpr std::size_t CdbRecordHeaderSize = 0x400;
constexpr std::size_t CdbConsoleIdSize = 8;

// The type names of the records of the Message Board.
constexpr std::string_view BoardRecordTypeName = "ripl_board_record";
constexpr std::string_view PlayLogRecordTypeName = "playtimelog";

// One file of the content database (CDB) of the System Menu, the library that keeps the Message Board in cdb.vff
// (CDBRecord.c, the header writer 0x81490828 and its check 0x81490644): a header of 0x400 bytes, then the data of the record.
struct CdbRecordFile
{
    // 2 for the files the System Menu 4.3 writes. Version 1 keeps the length of the type name at another place.
    Byte version = 2;

    // The first 8 bytes of the SHA-1 of the console's wireless address and "uyy" (cdbwiiid.dat): the console that made the record.
    std::array<Byte, CdbConsoleIdSize> consoleId{};

    std::string typeName;

    // The value of the counter file cdb.conf when the record was made: the first record of a database has 0.
    std::uint32_t recordNumber = 0;

    // 1 when the record is made, one more for every change of its data.
    std::uint32_t writeCount = 1;

    // Only in a copy on the SD card: the size of the record before it was encrypted. Zero on the NAND.
    std::uint32_t sizeOnSdCard = 0;

    // Seconds since the start of the year 2000, local time.
    std::uint32_t modifiedTime = 0;

    // The rest of the header (0x80 to 0x3FF): zeros on the NAND; the key, the IV and the HMAC of a copy on the SD card.
    std::array<Byte, CdbRecordHeaderSize - 0x80> restOfHeader{};

    Bytes data;
};

// Throws a FormatError when the file does not start with "CDBFILE", has a version above 2 or a type name without an end.
CdbRecordFile ReadCdbRecordFile(ByteView bytes);

// Writes the header of version 2 and the data.
Bytes WriteCdbRecordFile(const CdbRecordFile& record);

}
