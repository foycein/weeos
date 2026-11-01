#include "Formats/ZipArchive.h"

#include "Formats/ByteReader.h"
#include "Formats/Crc32.h"
#include "Formats/FormatError.h"
#include "Formats/Inflate.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view ArchiveDescription = "Zip archive";

constexpr std::uint32_t EndOfDirectorySignature = 0x06054B50;
constexpr std::uint32_t DirectoryEntrySignature = 0x02014B50;
constexpr std::uint32_t LocalHeaderSignature = 0x04034B50;

constexpr std::size_t EndOfDirectorySize = 22;
constexpr std::size_t LongestArchiveComment = 0xFFFF;

// The local header of a file: its fields up to the length of its name.
constexpr std::size_t LocalHeaderFieldsBeforeNameLength = 26;

// Fields of an entry of the central directory that are not needed: the versions that made the archive and that it needs, the time and
// date of the file, its disk number and its attributes.
constexpr std::size_t VersionFieldsSize = 4;
constexpr std::size_t TimeAndDateSize = 4;
constexpr std::size_t DiskAndAttributesSize = 8;
constexpr std::size_t DirectorySizeField = 4;

constexpr std::uint16_t StoredMethod = 0;
constexpr std::uint16_t DeflateMethod = 8;
constexpr std::uint16_t EncryptedFlag = 0x0001;

[[noreturn]] void Fail(const std::string& problem)
{
    throw FormatError(std::format("{}: {}", ArchiveDescription, problem));
}

std::uint32_t ReadSignatureAt(ByteView zipBytes, std::size_t offset)
{
    ByteReader reader(zipBytes, ArchiveDescription);
    reader.MoveTo(offset);
    return reader.ReadLittleEndianUint32();
}

// The end of the central directory is the last record of the archive, followed only by the comment of the archive.
std::size_t FindEndOfDirectory(ByteView zipBytes)
{
    if (zipBytes.size() < EndOfDirectorySize)
        Fail("too small to be a zip archive");

    std::size_t lastPossibleStart = zipBytes.size() - EndOfDirectorySize;
    std::size_t searchLength = std::min(lastPossibleStart, LongestArchiveComment);
    for (std::size_t distance = 0; distance <= searchLength; ++distance)
    {
        std::size_t start = lastPossibleStart - distance;
        if (ReadSignatureAt(zipBytes, start) == EndOfDirectorySignature)
            return start;
    }

    Fail("the end of the central directory is missing");
}

ByteView FindPackedBytes(ByteView zipBytes, std::uint32_t localHeaderOffset, std::uint32_t packedSize)
{
    ByteReader reader(zipBytes, ArchiveDescription);
    reader.MoveTo(localHeaderOffset);
    if (reader.ReadLittleEndianUint32() != LocalHeaderSignature)
        Fail("a file does not start with a local header");

    reader.MoveTo(localHeaderOffset + LocalHeaderFieldsBeforeNameLength);
    std::uint16_t nameLength = reader.ReadLittleEndianUint16();
    std::uint16_t extraLength = reader.ReadLittleEndianUint16();
    reader.Skip(nameLength);
    reader.Skip(extraLength);
    return reader.ReadBytes(packedSize);
}

ZipEntry ReadDirectoryEntry(ByteReader& reader, ByteView zipBytes)
{
    if (reader.ReadLittleEndianUint32() != DirectoryEntrySignature)
        Fail("an entry of the central directory has no signature");

    reader.Skip(VersionFieldsSize);
    std::uint16_t flags = reader.ReadLittleEndianUint16();

    ZipEntry entry;
    entry.compressionMethod = reader.ReadLittleEndianUint16();
    reader.Skip(TimeAndDateSize);
    entry.crc32 = reader.ReadLittleEndianUint32();
    std::uint32_t packedSize = reader.ReadLittleEndianUint32();
    entry.unpackedSize = reader.ReadLittleEndianUint32();
    std::uint16_t nameLength = reader.ReadLittleEndianUint16();
    std::uint16_t extraLength = reader.ReadLittleEndianUint16();
    std::uint16_t commentLength = reader.ReadLittleEndianUint16();
    reader.Skip(DiskAndAttributesSize);
    std::uint32_t localHeaderOffset = reader.ReadLittleEndianUint32();

    entry.name = reader.ReadText(nameLength);
    reader.Skip(extraLength);
    reader.Skip(commentLength);

    if ((flags & EncryptedFlag) != 0)
        Fail(std::format("{} is encrypted", entry.name));

    if (entry.compressionMethod != StoredMethod && entry.compressionMethod != DeflateMethod)
        Fail(std::format("{} is packed with method {}, which is not read", entry.name, entry.compressionMethod));

    entry.packedBytes = FindPackedBytes(zipBytes, localHeaderOffset, packedSize);
    return entry;
}

}

bool IsFolder(const ZipEntry& entry)
{
    return entry.name.ends_with('/');
}

std::vector<ZipEntry> ReadZipEntries(ByteView zipBytes)
{
    ByteReader endReader(zipBytes, ArchiveDescription);
    endReader.MoveTo(FindEndOfDirectory(zipBytes));
    endReader.Skip(sizeof(EndOfDirectorySignature));
    std::uint16_t diskNumber = endReader.ReadLittleEndianUint16();
    std::uint16_t diskOfDirectory = endReader.ReadLittleEndianUint16();
    std::uint16_t entriesOnThisDisk = endReader.ReadLittleEndianUint16();
    std::uint16_t entryCount = endReader.ReadLittleEndianUint16();
    endReader.Skip(DirectorySizeField);
    std::uint32_t directoryOffset = endReader.ReadLittleEndianUint32();

    if (diskNumber != 0 || diskOfDirectory != 0 || entriesOnThisDisk != entryCount)
        Fail("the archive is split into parts");

    ByteReader directoryReader(zipBytes, ArchiveDescription);
    directoryReader.MoveTo(directoryOffset);

    std::vector<ZipEntry> entries;
    for (int entryNumber = 0; entryNumber < entryCount; ++entryNumber)
        entries.push_back(ReadDirectoryEntry(directoryReader, zipBytes));

    return entries;
}

Bytes UnpackZipEntry(const ZipEntry& entry)
{
    Bytes unpacked;
    if (entry.compressionMethod == DeflateMethod)
        unpacked = Inflate(entry.packedBytes);
    else
        unpacked.assign(entry.packedBytes.begin(), entry.packedBytes.end());

    if (unpacked.size() != entry.unpackedSize)
        Fail(std::format("{} unpacks to {} bytes instead of {}", entry.name, unpacked.size(), entry.unpackedSize));

    if (ComputeCrc32(unpacked) != entry.crc32)
        Fail(std::format("{} is damaged: its CRC-32 does not match", entry.name));

    return unpacked;
}

}
