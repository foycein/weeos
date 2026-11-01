#pragma once

#include "Formats/Aes128.h"
#include "Formats/BannerContent.h"
#include "Formats/Bytes.h"
#include "Formats/Md5.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Formats
{

// The System Menu keeps two kinds of files for a title on an SD card, in the folder /private/wii/title/<code>/:
// data.bin holds the save data of the title and content.bin holds a channel. Each file starts with a header that the console
// encrypts with its SD key, and a backup archive follows: a header, the title metadata and the contents of a channel,
// or the files of the save data, and at the end the signature of the console.
//
// All numbers are big endian. docs/audit/SystemMenu.md (Data Management) says where every field comes from.

// The initialization vector that goes with the SD key of the console. Every encrypted header starts a chain of its own with it.
inline constexpr AesBlock SdCardInitializationVector = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// A header keeps the MD5 digest of itself. The digest is computed while these bytes stand in its place.
inline constexpr Md5Digest HeaderDigestPlaceholder = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// The header of data.bin: the title, and the file banner.bin of its save data with the rights and the flags it had on the NAND.
constexpr std::size_t SaveBackupHeaderSize = 0xF0C0;

struct SaveBackupHeader
{
    std::uint64_t titleId = 0;

    // The rights of banner.bin as the NAND library writes them: 0x20 owner may write, 0x10 owner may read, 0x08 and 0x04 the same
    // for the group, 0x02 and 0x01 for everybody else.
    std::uint8_t bannerPermissions = 0;
    std::uint8_t bannerFlags = 0;
    Bytes banner;
};

// The plain header with its digest. Throws a FormatError when the banner is too big.
Bytes WriteSaveBackupHeader(const SaveBackupHeader& header);

// Reads a decrypted header. Throws a FormatError when the digest is wrong or the banner is no save banner.
SaveBackupHeader ReadSaveBackupHeader(ByteView plainHeader);

// The header of content.bin: the title, the icon of the channel and the header of its banner, which holds the names of the channel.
constexpr std::size_t ChannelBackupHeaderSize = 0x640;

// The header of the banner content (IMET) as content.bin keeps it: 0x40 empty bytes, then the header itself. At most 0x600 bytes.
constexpr std::size_t BannerHeaderOffsetInContent = 0x40;
constexpr std::size_t BiggestBannerHeaderSize = 0x600;

// The icon (the file meta/icon.bin of the banner) follows the header, encrypted with a chain of its own. It has at most 0xC800 bytes.
constexpr std::size_t BiggestChannelIconSize = 0xC800;

struct ChannelBackupHeader
{
    std::uint64_t titleId = 0;
    std::uint32_t iconSize = 0;

    // The digest of the icon with the bytes that fill it up to a multiple of 64.
    Md5Digest iconDigest = {};

    Bytes bannerHeader;
};

// The plain header with its digest. Throws a FormatError when the banner header is too big.
Bytes WriteChannelBackupHeader(const ChannelBackupHeader& header);

// Reads a decrypted header. Throws a FormatError when the digest is wrong.
ChannelBackupHeader ReadChannelBackupHeader(ByteView plainHeader);

// The names of a channel in the ten languages, from the banner header that content.bin keeps.
// Throws a FormatError when the bytes are no banner header.
std::array<ChannelName, BannerLanguageCount> ReadChannelNames(ByteView bannerHeader);

// The parts of the backup archive start on multiples of 64 bytes.
constexpr std::size_t BackupAlignment = 0x40;

constexpr std::size_t BackupArchiveHeaderSize = 0x80;
constexpr std::size_t BackupFileHeaderSize = 0x80;

// The end of the archive: the signature in 0x40 bytes, the certificate of the console and the certificate of the key that signed.
constexpr std::size_t BackupTrailerSize = 0x340;
constexpr std::size_t BackupSignatureSlotSize = 0x40;
constexpr std::size_t BackupCertificateSize = 0x180;

// One bit for each content of the title metadata, in its order, the lowest bit of a byte first. A set bit means the content is in the archive.
constexpr std::size_t BackupContentBitmapSize = 0x40;

constexpr std::size_t TransferIdSize = 6;

struct BackupArchiveHeader
{
    // The console that made the archive.
    std::uint32_t deviceId = 0;

    std::uint32_t fileCount = 0;

    // The files with their headers: for each, 0x80 bytes and its data filled up to a multiple of 64.
    std::uint32_t filesSize = 0;

    std::uint32_t titleMetadataSize = 0;

    // The contents, each filled up to a multiple of 64.
    std::uint32_t contentsSize = 0;

    // Everything from the header to the end of the trailer.
    std::uint32_t totalSize = 0;

    std::array<Byte, BackupContentBitmapSize> contentBitmap = {};

    // The title of the save data, and the first bytes of /shared2/succession/transfer.id of the console. Zero in a channel.
    std::uint64_t titleId = 0;
    std::array<Byte, TransferIdSize> transferId = {};
};

Bytes WriteBackupArchiveHeader(const BackupArchiveHeader& header);

// Throws a FormatError when the bytes are no header of a backup archive.
BackupArchiveHeader ReadBackupArchiveHeader(ByteView bytes);

// The size of the whole archive.
std::uint32_t BackupArchiveTotalSize(const BackupArchiveHeader& header);

enum class BackupEntryKind : Byte
{
    File = 1,
    Directory = 2,
};

// The longest path of a file in the archive, without the zero byte that ends it.
constexpr std::size_t LongestBackupFileName = 0x3F;

// A file or a directory of the save data. The name is the path below the data folder of the title, for example "dir/file".
// The data of a file follows its header, encrypted with the SD key and the initialization vector of the header.
struct BackupFileHeader
{
    std::uint32_t size = 0;
    std::uint8_t permissions = 0;
    std::uint8_t flags = 0;
    BackupEntryKind kind = BackupEntryKind::File;
    std::string name;
    AesBlock initializationVector = {};
};

// The bytes behind the name are filled with the given bytes, and the initialization vector is written in its place among them.
// Throws a FormatError when the name is too long.
Bytes WriteBackupFileHeader(const BackupFileHeader& header, ByteView fillBytes);

// Throws a FormatError when the bytes are no header of a file.
BackupFileHeader ReadBackupFileHeader(ByteView bytes);

// The random numbers of the C library the System Menu was built with. The console fills the gaps of a backup archive with them
// and takes the initialization vectors of the files from them, starting from the low half of the title id.
class BackupFillNumbers
{
public:
    explicit BackupFillNumbers(std::uint32_t seed);

    Bytes NextBytes(std::size_t count);

private:
    std::uint32_t NextNumber();
    std::uint32_t NextWord();

    std::uint32_t state;
};

}
