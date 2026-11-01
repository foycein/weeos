#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"

#include <string_view>

namespace Formats
{

// The data of a copy is encrypted in pieces of this size; the last piece is filled with zeros.
constexpr std::size_t CdbSdCopyPieceSize = 0x40;

// The places in the header of a copy (CDBRecord.c, 0x8148D328 and the setters 0x81490AE0, 0x81490B80, 0x81490D2C, 0x81490D1C).
constexpr std::size_t CdbSdCopyFileSizeOffset = 0x78;
constexpr std::size_t CdbSdCopyRecordKeyOffset = 0x80;
constexpr std::size_t CdbSdCopyRecordKeySize = 0x20;
constexpr std::size_t CdbSdCopyInitializationVectorOffset = 0xA0;
constexpr std::size_t CdbSdCopyHmacOffset = 0xB0;
constexpr std::size_t CdbSdCopyHmacAreaSize = 0x1BC;

// The copy of a record of the Message Board's database on the SD card, which the menu makes when it moves old messages off a full
// board (0x8148CF24): the header of the record with the size of the whole record file at 0x78, the key of the record at 0x80
// ("0752022178_HAEA_#1_000.txt"), the initialization vector at 0xA0 and an HMAC-SHA1 at 0xB0; then the data of the record,
// encrypted with AES-128-CBC under a key of 16 zero bytes in pieces of 0x40 bytes. The HMAC is taken over the whole copy while
// 0xB0 to 0x26B hold zeros.
// Throws a FormatError when the record file is shorter than its header or the key does not fit.
Bytes MakeCdbSdCopy(ByteView recordFile, std::string_view recordKey, const AesBlock& initializationVector, ByteView hmacKey);

}
