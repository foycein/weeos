#include "Formats/Boot2Image.h"

#include "Formats/Aes128.h"
#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::uint32_t ExpectedHeaderSize = 0x20;
constexpr std::uint32_t DataAlignment = 0x40;
constexpr std::uint32_t LargestDataOffset = 0x20000;
constexpr std::size_t HeaderPaddingSize = 12;

struct Boot2Header
{
    std::uint32_t dataOffset;
    std::uint32_t certificateChainSize;
    std::uint32_t ticketSize;
    std::uint32_t titleMetadataSize;
};

// The checks of the header are the ones that boot1 makes before it accepts boot2.
Boot2Header ReadBoot2Header(ByteReader& reader)
{
    std::uint32_t headerSize = reader.ReadUint32();
    if (headerSize != ExpectedHeaderSize)
        reader.Fail(std::format("the header size {:#x} is not {:#x}", headerSize, ExpectedHeaderSize));

    Boot2Header header;
    header.dataOffset = reader.ReadUint32();
    header.certificateChainSize = reader.ReadUint32();
    header.ticketSize = reader.ReadUint32();
    header.titleMetadataSize = reader.ReadUint32();
    reader.Skip(HeaderPaddingSize);

    if (header.dataOffset > LargestDataOffset)
        reader.Fail(std::format("the data offset {:#x} is larger than {:#x}", header.dataOffset, LargestDataOffset));

    if (header.dataOffset % DataAlignment != 0)
        reader.Fail(std::format("the data offset {:#x} is not a multiple of {}", header.dataOffset, DataAlignment));

    std::uint64_t sectionsSize = std::uint64_t{header.certificateChainSize} + header.ticketSize + header.titleMetadataSize;
    std::uint64_t sectionsEnd = ExpectedHeaderSize + sectionsSize;
    if (sectionsEnd > header.dataOffset)
        reader.Fail(std::format("the sections end at {:#x}, after the data offset {:#x}", sectionsEnd, header.dataOffset));

    return header;
}

// boot2 has one content. Its data starts at the data offset.
Bytes ReadEncryptedContent(ByteReader& reader, std::uint32_t dataOffset, const TitleMetadata& titleMetadata)
{
    if (titleMetadata.contents.size() != 1)
        reader.Fail(std::format("the title metadata lists {} contents but boot2 has exactly one", titleMetadata.contents.size()));

    reader.MoveTo(dataOffset);
    std::uint64_t encryptedSize = RoundUpToMultiple(titleMetadata.contents.front().size, AesBlockSize);
    ByteView encryptedContent = reader.ReadBytes(static_cast<std::size_t>(encryptedSize));
    return Bytes(encryptedContent.begin(), encryptedContent.end());
}

}

Boot2Image ReadBoot2Image(ByteView bytes)
{
    ByteReader reader(bytes, "boot2 image");
    Boot2Header header = ReadBoot2Header(reader);

    Boot2Image image;
    image.certificates = ReadCertificateChain(reader.ReadBytes(header.certificateChainSize));
    image.ticket = ReadTicket(reader.ReadBytes(header.ticketSize));
    image.titleMetadata = ReadTitleMetadata(reader.ReadBytes(header.titleMetadataSize));
    image.encryptedContent = ReadEncryptedContent(reader, header.dataOffset, image.titleMetadata);
    return image;
}

}
