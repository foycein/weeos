#include "Formats/SdCardBackup.h"

#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"
#include "Formats/SaveBanner.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace Formats
{

namespace
{

constexpr std::size_t SaveBackupBannerOffset = 0x20;
constexpr std::size_t SaveBackupDigestOffset = 0x0E;
constexpr std::size_t ChannelBackupHeaderDigestOffset = 0x0C;
constexpr std::size_t ChannelBackupBannerHeaderOffset = 0x40;

constexpr std::string_view BannerTag = "IMET";
constexpr std::size_t BannerTagOffset = 0x40;
constexpr std::size_t ChannelNamesOffset = BannerTagOffset + 0x1C;
constexpr std::size_t ChannelNameLineLength = 21;

constexpr std::uint32_t ArchiveHeaderLength = 0x70;
constexpr std::string_view ArchiveTag = "Bk";
constexpr std::uint16_t ArchiveVersion = 1;
constexpr std::size_t ArchiveTitleIdOffset = 0x60;

constexpr std::string_view ChannelPlacesTag = "sdal";
constexpr std::size_t ChannelPlacesDigestOffset = 0x04;

constexpr std::uint32_t FileHeaderMagic = 0x03ADF17E;
constexpr std::size_t FileNameOffset = 0x0B;
constexpr std::size_t FileNameFieldSize = 0x40;
constexpr std::size_t FileInitializationVectorOffset = 0x50;

constexpr std::uint32_t RandomMultiplier = 0x41C64E6D;
constexpr std::uint32_t RandomIncrement = 0x3039;
constexpr int RandomResultShift = 16;
constexpr std::uint32_t RandomResultMask = 0x7FFF;
constexpr int MiddleBitsShift = 2;
constexpr int HighBitsShift = 17;
constexpr std::uint32_t LowBitsMask = 0x3;

// Computes the digest of a plain header with the placeholder in the place of the digest.
Md5Digest DigestOfHeader(ByteView plainHeader, std::size_t digestOffset)
{
    Bytes copy(plainHeader.begin(), plainHeader.end());
    std::copy(HeaderDigestPlaceholder.begin(), HeaderDigestPlaceholder.end(), copy.begin() + digestOffset);
    return ComputeMd5(copy);
}

void PutDigest(Bytes& plainHeader, std::size_t digestOffset)
{
    Md5Digest digest = DigestOfHeader(plainHeader, digestOffset);
    std::copy(digest.begin(), digest.end(), plainHeader.begin() + digestOffset);
}

void CheckDigest(ByteReader& reader, ByteView plainHeader, std::size_t digestOffset)
{
    Md5Digest expected = DigestOfHeader(plainHeader, digestOffset);
    ByteView stored = plainHeader.subspan(digestOffset, Md5DigestSize);
    if (!std::equal(stored.begin(), stored.end(), expected.begin()))
        reader.Fail("the MD5 digest of the header is wrong");
}

Bytes PadWithZeros(const ByteWriter& writer, std::size_t size)
{
    Bytes bytes = writer.GetBytes();
    bytes.resize(size, 0);
    return bytes;
}

std::u16string ReadNameLine(ByteReader& reader)
{
    std::u16string line;
    bool hasEnded = false;

    for (std::size_t index = 0; index < ChannelNameLineLength; ++index)
    {
        char16_t character = static_cast<char16_t>(reader.ReadUint16());
        if (character == 0)
            hasEnded = true;

        if (!hasEnded)
            line.push_back(character);
    }
    return line;
}

}

Bytes WriteSaveBackupHeader(const SaveBackupHeader& header)
{
    std::size_t room = SaveBackupHeaderSize - SaveBackupBannerOffset;
    if (header.banner.size() > room)
        throw FormatError(std::format("A save banner of {:#x} bytes does not fit into the header of data.bin", header.banner.size()));

    ByteWriter writer;
    writer.WriteUint64(header.titleId);
    writer.WriteUint32(static_cast<std::uint32_t>(header.banner.size()));
    writer.WriteByte(header.bannerPermissions);
    writer.WriteByte(header.bannerFlags);
    writer.WriteZeros(Md5DigestSize);
    writer.WriteUint16(0);
    writer.WriteBytes(header.banner);

    Bytes plainHeader = PadWithZeros(writer, SaveBackupHeaderSize);
    PutDigest(plainHeader, SaveBackupDigestOffset);
    return plainHeader;
}

SaveBackupHeader ReadSaveBackupHeader(ByteView plainHeader)
{
    ByteReader reader(plainHeader, "Header of data.bin");
    if (plainHeader.size() != SaveBackupHeaderSize)
        reader.Fail(std::format("the header has {:#x} bytes instead of {:#x}", plainHeader.size(), SaveBackupHeaderSize));

    CheckDigest(reader, plainHeader, SaveBackupDigestOffset);

    SaveBackupHeader header;
    header.titleId = reader.ReadUint64();
    std::uint32_t bannerSize = reader.ReadUint32();
    header.bannerPermissions = reader.ReadByte();
    header.bannerFlags = reader.ReadByte();

    if (bannerSize > SaveBackupHeaderSize - SaveBackupBannerOffset)
        reader.Fail(std::format("the banner of {:#x} bytes does not fit into the header", bannerSize));

    reader.MoveTo(SaveBackupBannerOffset);
    ByteView banner = reader.ReadBytes(bannerSize);
    header.banner = Bytes(banner.begin(), banner.end());

    ReadSaveBanner(header.banner);
    return header;
}

Bytes WriteChannelBackupHeader(const ChannelBackupHeader& header)
{
    if (header.bannerHeader.size() > BiggestBannerHeaderSize)
        throw FormatError(std::format("A banner header of {:#x} bytes does not fit into the header of content.bin", header.bannerHeader.size()));

    ByteWriter writer;
    writer.WriteUint64(header.titleId);
    writer.WriteUint32(header.iconSize);
    writer.WriteZeros(Md5DigestSize);
    writer.WriteBytes(header.iconDigest);
    writer.WriteZeros(ChannelBackupBannerHeaderOffset - writer.GetBytes().size());
    writer.WriteBytes(header.bannerHeader);

    Bytes plainHeader = PadWithZeros(writer, ChannelBackupHeaderSize);
    PutDigest(plainHeader, ChannelBackupHeaderDigestOffset);
    return plainHeader;
}

ChannelBackupHeader ReadChannelBackupHeader(ByteView plainHeader)
{
    ByteReader reader(plainHeader, "Header of content.bin");
    if (plainHeader.size() != ChannelBackupHeaderSize)
        reader.Fail(std::format("the header has {:#x} bytes instead of {:#x}", plainHeader.size(), ChannelBackupHeaderSize));

    CheckDigest(reader, plainHeader, ChannelBackupHeaderDigestOffset);

    ChannelBackupHeader header;
    header.titleId = reader.ReadUint64();
    header.iconSize = reader.ReadUint32();
    reader.Skip(Md5DigestSize);
    header.iconDigest = reader.ReadByteArray<Md5DigestSize>();

    if (header.iconSize > BiggestChannelIconSize)
        reader.Fail(std::format("an icon of {:#x} bytes is bigger than {:#x}", header.iconSize, BiggestChannelIconSize));

    reader.MoveTo(ChannelBackupBannerHeaderOffset);
    ByteView bannerHeader = reader.ReadBytes(BiggestBannerHeaderSize);
    header.bannerHeader = Bytes(bannerHeader.begin(), bannerHeader.end());
    return header;
}

std::array<ChannelName, BannerLanguageCount> ReadChannelNames(ByteView bannerHeader)
{
    ByteReader reader(bannerHeader, "Banner header");

    reader.MoveTo(BannerTagOffset);
    std::string tag = reader.ReadText(BannerTag.size());
    if (tag != BannerTag)
        reader.Fail(std::format("the tag {} is not {}", tag, BannerTag));

    reader.MoveTo(ChannelNamesOffset);
    std::array<ChannelName, BannerLanguageCount> names;
    for (ChannelName& name : names)
    {
        name.firstLine = ReadNameLine(reader);
        name.secondLine = ReadNameLine(reader);
    }
    return names;
}

Bytes WriteBackupArchiveHeader(const BackupArchiveHeader& header)
{
    ByteWriter writer;
    writer.WriteUint32(ArchiveHeaderLength);
    writer.WriteBytes(ByteView(reinterpret_cast<const Byte*>(ArchiveTag.data()), ArchiveTag.size()));
    writer.WriteUint16(ArchiveVersion);
    writer.WriteUint32(header.deviceId);
    writer.WriteUint32(header.fileCount);
    writer.WriteUint32(header.filesSize);
    writer.WriteUint32(header.titleMetadataSize);
    writer.WriteUint32(header.contentsSize);
    writer.WriteUint32(header.totalSize);
    writer.WriteBytes(header.contentBitmap);
    writer.WriteUint64(header.titleId);
    writer.WriteBytes(header.transferId);
    return PadWithZeros(writer, BackupArchiveHeaderSize);
}

BackupArchiveHeader ReadBackupArchiveHeader(ByteView bytes)
{
    ByteReader reader(bytes, "Header of a backup archive");

    std::uint32_t length = reader.ReadUint32();
    std::string tag = reader.ReadText(ArchiveTag.size());
    std::uint16_t version = reader.ReadUint16();
    if (length != ArchiveHeaderLength || tag != ArchiveTag || version != ArchiveVersion)
        reader.Fail(std::format("the header is not a header of version {} with {:#x} bytes and the tag {}", ArchiveVersion, ArchiveHeaderLength, ArchiveTag));

    BackupArchiveHeader header;
    header.deviceId = reader.ReadUint32();
    header.fileCount = reader.ReadUint32();
    header.filesSize = reader.ReadUint32();
    header.titleMetadataSize = reader.ReadUint32();
    header.contentsSize = reader.ReadUint32();
    header.totalSize = reader.ReadUint32();
    header.contentBitmap = reader.ReadByteArray<BackupContentBitmapSize>();

    reader.MoveTo(ArchiveTitleIdOffset);
    header.titleId = reader.ReadUint64();
    header.transferId = reader.ReadByteArray<TransferIdSize>();
    return header;
}

std::uint32_t BackupArchiveTotalSize(const BackupArchiveHeader& header)
{
    std::uint64_t titleMetadataPart = RoundUpToMultiple(header.titleMetadataSize, BackupAlignment);
    std::uint64_t total = BackupArchiveHeaderSize + titleMetadataPart + header.contentsSize + header.filesSize + BackupTrailerSize;
    return static_cast<std::uint32_t>(total);
}

Bytes WriteBackupFileHeader(const BackupFileHeader& header, ByteView fillBytes)
{
    if (header.name.size() > LongestBackupFileName)
        throw FormatError(std::format("The name {} is longer than {} characters", header.name, LongestBackupFileName));

    ByteWriter writer;
    writer.WriteUint32(FileHeaderMagic);
    writer.WriteUint32(header.size);
    writer.WriteByte(header.permissions);
    writer.WriteByte(header.flags);
    writer.WriteByte(static_cast<Byte>(header.kind));
    writer.WriteBytes(ByteView(reinterpret_cast<const Byte*>(header.name.data()), header.name.size()));
    writer.WriteByte(0);

    std::size_t fillSize = BackupFileHeaderSize - writer.GetBytes().size();
    writer.WriteBytes(fillBytes.first(std::min(fillSize, fillBytes.size())));

    Bytes bytes = PadWithZeros(writer, BackupFileHeaderSize);
    std::copy(header.initializationVector.begin(), header.initializationVector.end(), bytes.begin() + FileInitializationVectorOffset);
    return bytes;
}

BackupFileHeader ReadBackupFileHeader(ByteView bytes)
{
    ByteReader reader(bytes, "Header of a file in a backup archive");

    std::uint32_t magic = reader.ReadUint32();
    if (magic != FileHeaderMagic)
        reader.Fail(std::format("the magic number {:#010x} is not {:#010x}", magic, FileHeaderMagic));

    BackupFileHeader header;
    header.size = reader.ReadUint32();
    header.permissions = reader.ReadByte();
    header.flags = reader.ReadByte();

    Byte kind = reader.ReadByte();
    if (kind != static_cast<Byte>(BackupEntryKind::File) && kind != static_cast<Byte>(BackupEntryKind::Directory))
        reader.Fail(std::format("the kind {} is neither a file nor a directory", kind));
    header.kind = static_cast<BackupEntryKind>(kind);

    reader.MoveTo(FileNameOffset);
    header.name = reader.ReadText(FileNameFieldSize);

    reader.MoveTo(FileInitializationVectorOffset);
    header.initializationVector = reader.ReadByteArray<AesBlockSize>();
    return header;
}

Bytes WriteChannelPlaces(const ChannelPlaces& places)
{
    ByteWriter writer;
    writer.WriteBytes(ByteView(reinterpret_cast<const Byte*>(ChannelPlacesTag.data()), ChannelPlacesTag.size()));
    writer.WriteZeros(Md5DigestSize);
    for (std::uint32_t code : places)
        writer.WriteUint32(code);

    Bytes file = PadWithZeros(writer, ChannelPlacesFileSize);
    PutDigest(file, ChannelPlacesDigestOffset);
    return file;
}

ChannelPlaces ReadChannelPlaces(ByteView plainFile)
{
    ByteReader reader(plainFile, "Places of the channels of the SD card");
    if (plainFile.size() != ChannelPlacesFileSize)
        reader.Fail(std::format("the file has {:#x} bytes instead of {:#x}", plainFile.size(), ChannelPlacesFileSize));

    CheckDigest(reader, plainFile, ChannelPlacesDigestOffset);

    reader.MoveTo(ChannelPlacesDigestOffset + Md5DigestSize);
    ChannelPlaces places;
    for (std::uint32_t& code : places)
        code = reader.ReadUint32();

    return places;
}

BackupFillNumbers::BackupFillNumbers(std::uint32_t seed)
    : state(seed)
{
}

Bytes BackupFillNumbers::NextBytes(std::size_t count)
{
    ByteWriter writer;
    while (writer.GetBytes().size() < count)
        writer.WriteUint32(NextWord());

    Bytes bytes = writer.GetBytes();
    bytes.resize(count);
    return bytes;
}

std::uint32_t BackupFillNumbers::NextNumber()
{
    state = state * RandomMultiplier + RandomIncrement;
    return (state >> RandomResultShift) & RandomResultMask;
}

// One number gives 15 bits, so a word is made of three: bits 2 to 16, bits 17 to 31 and bits 0 and 1.
std::uint32_t BackupFillNumbers::NextWord()
{
    std::uint32_t middleBits = NextNumber() << MiddleBitsShift;
    std::uint32_t highBits = NextNumber() << HighBitsShift;
    std::uint32_t lowBits = NextNumber() & LowBitsMask;
    return highBits | middleBits | lowBits;
}

}
