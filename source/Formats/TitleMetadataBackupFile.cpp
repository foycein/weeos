#include "Formats/TitleMetadataBackupFile.h"

#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"

namespace Formats
{

namespace
{

constexpr std::size_t HeaderZeroCount = 0x14;

}

std::vector<TitleMetadataBackup> ReadTitleMetadataBackups(ByteView file)
{
    ByteReader reader(file, "Title metadata backup");

    std::vector<TitleMetadataBackup> backups;
    while (!reader.IsAtEnd())
    {
        std::size_t recordStart = reader.Position();

        TitleMetadataBackup backup;
        backup.titleId = reader.ReadUint64();
        std::uint32_t size = reader.ReadUint32();

        reader.MoveTo(recordStart + TitleMetadataBackupHeaderSize);
        ByteView titleMetadata = reader.ReadBytes(size);
        backup.titleMetadata.assign(titleMetadata.begin(), titleMetadata.end());
        backups.push_back(backup);

        std::size_t paddedSize = RoundUpToMultiple(size, TitleMetadataBackupAlignment);
        reader.MoveTo(recordStart + TitleMetadataBackupHeaderSize + paddedSize);
    }
    return backups;
}

Bytes WriteTitleMetadataBackup(const TitleMetadataBackup& backup)
{
    ByteWriter record;
    record.WriteUint64(backup.titleId);
    record.WriteUint32(static_cast<std::uint32_t>(backup.titleMetadata.size()));
    record.WriteZeros(HeaderZeroCount);
    record.WriteBytes(backup.titleMetadata);

    std::size_t paddedSize = RoundUpToMultiple(backup.titleMetadata.size(), TitleMetadataBackupAlignment);
    record.WriteZeros(paddedSize - backup.titleMetadata.size());
    return record.GetBytes();
}

}
