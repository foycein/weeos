#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

// One file or folder of a zip archive. The name of a folder ends with "/".
struct ZipEntry
{
    std::string name;
    std::uint16_t compressionMethod = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t unpackedSize = 0;

    // The packed bytes of the file, inside the bytes of the archive.
    ByteView packedBytes;
};

bool IsFolder(const ZipEntry& entry);

// Reads the list of a zip archive (its central directory). The entries point into zipBytes, which must stay alive while they are used.
// Read are files that are stored or packed with deflate, the way the release of WeeOS writes its zip: no encryption, no zip64 and no
// archive split into parts. Throws a FormatError when the archive is damaged or uses anything else.
std::vector<ZipEntry> ReadZipEntries(ByteView zipBytes);

// Unpacks one file and checks its size and its CRC-32. Throws a FormatError when they do not match.
Bytes UnpackZipEntry(const ZipEntry& entry);

}
