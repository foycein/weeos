#include "Formats/WadFile.h"

#include "Formats/Aes128.h"
#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::uint32_t ExpectedHeaderSize = 0x20;
constexpr std::uint16_t InstallableType = 0x4973;
constexpr std::uint16_t BootTwoType = 0x6962;
constexpr std::uint16_t ExpectedVersion = 0;
constexpr std::uint64_t SectionAlignment = 0x40;

struct WadHeader
{
    std::uint32_t certificateChainSize;
    std::uint32_t ticketSize;
    std::uint32_t titleMetadataSize;
};

WadHeader ReadWadHeader(ByteReader& reader)
{
    std::uint32_t headerSize = reader.ReadUint32();
    if (headerSize != ExpectedHeaderSize)
        reader.Fail(std::format("the header size {:#x} is not {:#x}", headerSize, ExpectedHeaderSize));

    std::uint16_t type = reader.ReadUint16();
    if (type != InstallableType && type != BootTwoType)
        reader.Fail(std::format("the WAD type {:#x} is not an installable title", type));

    std::uint16_t version = reader.ReadUint16();
    if (version != ExpectedVersion)
        reader.Fail(std::format("the WAD version {} is not supported", version));

    WadHeader header;
    header.certificateChainSize = reader.ReadUint32();

    std::uint32_t revocationListSize = reader.ReadUint32();
    if (revocationListSize != 0)
        reader.Fail("a WAD with a revocation list is not supported");

    header.ticketSize = reader.ReadUint32();
    header.titleMetadataSize = reader.ReadUint32();
    return header;
}

// Every section of a WAD starts at a multiple of 64 bytes.
ByteView ReadSection(ByteReader& reader, std::uint64_t sectionSize)
{
    reader.MoveTo(static_cast<std::size_t>(RoundUpToMultiple(reader.Position(), SectionAlignment)));
    return reader.ReadBytes(static_cast<std::size_t>(sectionSize));
}

std::vector<Bytes> ReadEncryptedContents(ByteReader& reader, const TitleMetadata& titleMetadata)
{
    std::vector<Bytes> encryptedContents;

    for (const ContentRecord& record : titleMetadata.contents)
    {
        std::uint64_t encryptedSize = RoundUpToMultiple(record.size, AesBlockSize);
        ByteView encryptedContent = ReadSection(reader, encryptedSize);
        encryptedContents.emplace_back(encryptedContent.begin(), encryptedContent.end());
    }

    return encryptedContents;
}

}

WadFile ReadWadFile(ByteView bytes)
{
    ByteReader reader(bytes, "WAD file");
    WadHeader header = ReadWadHeader(reader);

    WadFile wad;
    wad.certificates = ReadCertificateChain(ReadSection(reader, header.certificateChainSize));
    wad.ticket = ReadTicket(ReadSection(reader, header.ticketSize));
    wad.titleMetadata = ReadTitleMetadata(ReadSection(reader, header.titleMetadataSize));
    wad.encryptedContents = ReadEncryptedContents(reader, wad.titleMetadata);
    return wad;
}

}
