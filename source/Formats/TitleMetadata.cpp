#include "Formats/TitleMetadata.h"

#include "Formats/ByteReader.h"

namespace Formats
{

namespace
{

constexpr std::size_t IssuerSize = 64;
constexpr std::size_t UnusedAfterGroupIdSize = 2;
constexpr std::size_t UnusedAfterRatingsSize = 12;
constexpr std::size_t UnusedAfterInputOutputMaskSize = 18;
constexpr std::size_t UnusedAfterBootIndexSize = 2;

ContentRecord ReadContentRecord(ByteReader& reader)
{
    ContentRecord record;
    record.id = reader.ReadUint32();
    record.index = reader.ReadUint16();
    record.type = reader.ReadUint16();
    record.size = reader.ReadUint64();
    record.hash = reader.ReadByteArray<Sha1DigestSize>();
    return record;
}

std::vector<ContentRecord> ReadContentRecords(ByteReader& reader, std::size_t contentCount)
{
    std::vector<ContentRecord> records;
    for (std::size_t recordNumber = 0; recordNumber < contentCount; ++recordNumber)
        records.push_back(ReadContentRecord(reader));

    return records;
}

}

TitleMetadata ReadTitleMetadata(ByteView bytes)
{
    ByteReader reader(bytes, "Title metadata");

    TitleMetadata metadata;
    metadata.signature = ReadSignature(reader);
    metadata.issuer = reader.ReadText(IssuerSize);
    metadata.version = reader.ReadByte();
    metadata.certificateAuthorityRevocationVersion = reader.ReadByte();
    metadata.signerRevocationVersion = reader.ReadByte();
    metadata.isVirtualWiiTitle = reader.ReadByte() != 0;
    metadata.requiredSystemVersion = reader.ReadUint64();
    metadata.titleId = reader.ReadUint64();
    metadata.titleType = reader.ReadUint32();
    metadata.groupId = reader.ReadUint16();
    reader.Skip(UnusedAfterGroupIdSize);
    metadata.region = static_cast<Region>(reader.ReadUint16());
    metadata.ratings = reader.ReadByteArray<TitleRatingsSize>();
    reader.Skip(UnusedAfterRatingsSize);
    metadata.inputOutputMask = reader.ReadByteArray<TitleInputOutputMaskSize>();
    reader.Skip(UnusedAfterInputOutputMaskSize);
    metadata.accessRights = reader.ReadUint32();
    metadata.titleVersion = reader.ReadUint16();
    std::uint16_t contentCount = reader.ReadUint16();
    metadata.bootIndex = reader.ReadUint16();
    reader.Skip(UnusedAfterBootIndexSize);
    metadata.contents = ReadContentRecords(reader, contentCount);

    return metadata;
}

}
