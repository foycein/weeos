#include "SystemMenu/DataManagement/ChannelBackup.h"

#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataLists.h"
#include "SystemMenu/DataManagement/NandRoom.h"
#include "SystemMenu/DataManagement/TitleKinds.h"

#include "Formats/Alignment.h"
#include "Formats/BannerContent.h"
#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view ChannelBackupFileName = "content.bin";
constexpr std::string_view IconFileName = "meta/icon.bin";
constexpr std::uint16_t BannerContentIndex = 0;

// The size of the banner header stands behind its four-byte tag, which is 0x40 bytes into the header.
constexpr std::size_t BannerTagOffsetInHeader = 0x40;
constexpr std::size_t BannerTagSize = 4;
constexpr std::size_t BannerHeaderSizeOffset = Formats::BannerHeaderOffsetInContent + BannerTagOffsetInHeader + BannerTagSize;

// The last bytes of the banner header are its MD5 digest. content.bin keeps the header with them and its first 0x40 bytes empty.
constexpr std::size_t BannerHeaderEmptyStart = 0x40;
constexpr Formats::Byte EmptyByte = 0;

// The console reads the icon in pieces of 32 bytes.
constexpr std::size_t IconReadAlignment = 0x20;

// Only the upper byte of the version counts when a channel goes back to the NAND.
constexpr int MajorVersionShift = 8;

struct InstalledChannelBanner
{
    Formats::Bytes bannerHeader;
    Formats::Bytes icon;
};

Formats::Bytes ReadBannerContent(const DataManagementServices& services, Ios::TitleId titleId)
{
    std::vector<Ios::TicketView> views = Require(services.titles.ListTicketViews(titleId), "read the tickets of the channel");
    if (views.empty())
        throw DataError(DataResult::Failed, "the channel has no ticket");

    Ios::ContentStream stream = Require(services.titles.OpenContent(titleId, views.front(), BannerContentIndex), "open the banner of the channel");
    return stream.Read(stream.GetLength());
}

Formats::Bytes CopyBannerHeader(Formats::ByteView content)
{
    Formats::ByteReader reader(content, "Banner content");
    reader.MoveTo(BannerHeaderSizeOffset);
    std::uint32_t headerSize = reader.ReadUint32();
    bool isTooBig = headerSize > Formats::BiggestBannerHeaderSize;
    bool isTooSmall = headerSize < BannerHeaderEmptyStart + Formats::Md5DigestSize;
    if (isTooBig || isTooSmall)
        reader.Fail(std::format("a banner header of {:#x} bytes does not fit into content.bin", headerSize));

    reader.MoveTo(Formats::BannerHeaderOffsetInContent);
    Formats::ByteView header = reader.ReadBytes(headerSize);

    Formats::Bytes copy(header.begin(), header.end());
    std::fill(copy.begin(), copy.begin() + BannerHeaderEmptyStart, EmptyByte);
    std::fill(copy.end() - Formats::Md5DigestSize, copy.end(), EmptyByte);
    return copy;
}

InstalledChannelBanner ReadInstalledChannelBanner(const DataManagementServices& services, Ios::TitleId titleId)
{
    Formats::Bytes content = ReadBannerContent(services, titleId);
    try
    {
        InstalledChannelBanner banner;
        banner.bannerHeader = CopyBannerHeader(content);

        Formats::BannerContent bannerContent = Formats::ReadBannerContent(content);
        std::optional<Formats::ByteView> icon = bannerContent.files.FindFile(IconFileName);
        if (!icon)
            throw DataError(DataResult::Failed, "the banner of the channel has no icon");

        if (Formats::RoundUpToMultiple(icon->size(), IconReadAlignment) > Formats::BiggestChannelIconSize)
            throw DataError(DataResult::Failed, "the icon of the channel is too big for content.bin");

        banner.icon = Formats::Bytes(icon->begin(), icon->end());
        return banner;
    }
    catch (const Formats::FormatError& error)
    {
        throw DataError(DataResult::Failed, error.what());
    }
}

Formats::Bytes IconBlockOf(Formats::ByteView icon)
{
    Formats::Bytes block(icon.begin(), icon.end());
    block.resize(Formats::RoundUpToMultiple(block.size(), Formats::BackupAlignment), 0);
    return block;
}

std::size_t ArchiveOffsetOf(const Formats::ChannelBackupHeader& header)
{
    return Formats::ChannelBackupHeaderSize + Formats::RoundUpToMultiple(header.iconSize, Formats::BackupAlignment);
}

DataResult CopyToCard(DataManagementServices& services, Ios::TitleId titleId)
{
    Ios::SdPath file = ChannelBackupPath(titleId.lowPart);
    PrepareBackupFile(services.sdCard, file, Formats::ChannelBackupHeaderSize);

    InstalledChannelBanner banner = ReadInstalledChannelBanner(services, titleId);
    Formats::Bytes archive = WriteChannelArchive(services, titleId);
    Formats::Bytes iconBlock = IconBlockOf(banner.icon);

    std::uint64_t freeBytes = RequireOnCard(services.sdCard.GetFreeBytes(), "measure the room on the SD card");
    std::uint64_t neededBytes = Formats::ChannelBackupHeaderSize + iconBlock.size() + archive.size();
    if (freeBytes < neededBytes)
        throw DataError(DataResult::NotEnoughRoom, "the SD card is full");

    Formats::ChannelBackupHeader header;
    header.titleId = titleId.ToValue();
    header.iconSize = static_cast<std::uint32_t>(banner.icon.size());
    header.iconDigest = Formats::ComputeMd5(iconBlock);
    header.bannerHeader = banner.bannerHeader;

    Formats::Bytes backup = EncryptForCard(services.identity, Formats::WriteChannelBackupHeader(header));
    Formats::Bytes encryptedIcon = EncryptForCard(services.identity, iconBlock);
    backup.insert(backup.end(), encryptedIcon.begin(), encryptedIcon.end());
    backup.insert(backup.end(), archive.begin(), archive.end());
    try
    {
        WriteSdFile(services.sdCard, file, backup);
    }
    catch (const DataError&)
    {
        DeleteSdFileIfPresent(services.sdCard, file);
        throw;
    }
    return DataResult::Done;
}

// Reads the header and the icon from the start of the file. The archive must follow them.
ChannelOnCard ReadHeaderAndIcon(const DataManagementServices& services, Formats::ByteView backupStart, std::uint64_t fileSize, std::uint32_t lowPart)
{
    if (backupStart.size() < Formats::ChannelBackupHeaderSize)
        throw DataError(DataResult::DamagedBackup, "content.bin is shorter than its header");

    ChannelOnCard channel;
    try
    {
        Formats::Bytes plainHeader = DecryptFromCard(services.identity, backupStart.first(Formats::ChannelBackupHeaderSize));
        channel.header = Formats::ReadChannelBackupHeader(plainHeader);
    }
    catch (const Formats::FormatError& error)
    {
        throw DataError(DataResult::DamagedBackup, error.what());
    }

    if (Ios::TitleId::FromValue(channel.header.titleId).lowPart != lowPart)
        throw DataError(DataResult::DamagedBackup, "content.bin belongs to another title");

    std::size_t archiveOffset = ArchiveOffsetOf(channel.header);
    if (fileSize <= archiveOffset || backupStart.size() < archiveOffset)
        throw DataError(DataResult::DamagedBackup, "content.bin ends before its archive");

    std::size_t iconBlockSize = archiveOffset - Formats::ChannelBackupHeaderSize;
    Formats::ByteView encryptedIconBlock = backupStart.subspan(Formats::ChannelBackupHeaderSize, iconBlockSize);
    Formats::Bytes iconBlock = DecryptFromCard(services.identity, encryptedIconBlock);
    if (Formats::ComputeMd5(iconBlock) != channel.header.iconDigest)
        throw DataError(DataResult::DamagedBackup, "the icon of content.bin is damaged");

    iconBlock.resize(channel.header.iconSize);
    channel.icon = iconBlock;
    return channel;
}

DataResult MapInstallationError(Ios::IosError error)
{
    if (error == Ios::IosError::HigherVersionInstalled)
        return DataResult::NewerVersionInstalled;

    return DataResult::DamagedBackup;
}

void Install(DataManagementServices& services, const BackupArchive& archive)
{
    Ios::Result<void> begun = services.titles.BeginInstallation(archive.titleMetadataBytes, {});
    if (begun.Failed())
        throw DataError(MapInstallationError(begun.Error()), "begin the installation");

    for (const ArchiveContent& content : archive.contents)
    {
        Ios::Result<void> added = services.titles.AddContent(content.record.id, content.encryptedData);
        if (added.Failed())
        {
            services.titles.CancelInstallation();
            throw DataError(DataResult::DamagedBackup, std::format("install the content {:08x}", content.record.id));
        }
    }

    Ios::Result<void> finished = services.titles.FinishInstallation();
    if (finished.Failed())
    {
        services.titles.CancelInstallation();
        throw DataError(DataResult::DamagedBackup, "finish the installation");
    }
}

NandRoomNeeded RoomNeededFor(const BackupArchive& archive)
{
    NandRoomNeeded needed;
    needed.userInodes = static_cast<std::uint32_t>(archive.contents.size());
    for (const ArchiveContent& content : archive.contents)
        needed.userClusters += ClustersOfBytes(content.record.size);

    return needed;
}

void CheckVersionAgainstNand(const DataManagementServices& services, Ios::TitleId titleId, const Formats::TitleMetadata& metadata)
{
    Ios::Result<Formats::TitleMetadata> installed = services.titles.ReadTitleMetadata(titleId);
    if (installed.Failed())
        return;

    int cardMajorVersion = metadata.titleVersion >> MajorVersionShift;
    int nandMajorVersion = installed.Value().titleVersion >> MajorVersionShift;
    if (cardMajorVersion < nandMajorVersion)
        throw DataError(DataResult::NewerVersionInstalled, "the NAND has a newer version of the channel");
}

Formats::TitleMetadata ReadArchiveMetadata(const BackupArchive& archive)
{
    try
    {
        return Formats::ReadTitleMetadata(archive.titleMetadataBytes);
    }
    catch (const Formats::FormatError& error)
    {
        throw DataError(DataResult::DamagedBackup, error.what());
    }
}

DataResult CopyToNand(DataManagementServices& services, std::uint32_t lowPart)
{
    if (ListNandChannels(services).size() >= ChannelSlotCount)
        throw DataError(DataResult::NotEnoughRoom, "every slot for a channel is taken");

    Formats::Bytes backup = ReadSdFile(services.sdCard, ChannelBackupPath(lowPart));
    ChannelOnCard channel = ReadHeaderAndIcon(services, backup, backup.size(), lowPart);

    Ios::TitleId titleId = Ios::TitleId::FromValue(channel.header.titleId);
    if (IsChannelComplete(services.titles, titleId))
        throw DataError(DataResult::AlreadyThere, "the channel is on the NAND");

    Ios::Result<std::vector<Ios::TicketView>> tickets = services.titles.ListTicketViews(titleId);
    if (tickets.Failed() || tickets.Value().empty())
        throw DataError(DataResult::TitleNotInstalled, "the NAND has no ticket for the channel");

    BackupArchive archive = ReadBackupArchive(Formats::ByteView(backup).subspan(ArchiveOffsetOf(channel.header)));
    Formats::TitleMetadata metadata = ReadArchiveMetadata(archive);
    if (metadata.titleId != channel.header.titleId)
        throw DataError(DataResult::DamagedBackup, "the archive holds another title than the header");

    CheckVersionAgainstNand(services, titleId, metadata);

    bool isFromAnotherConsole = archive.header.deviceId != services.identity.GetDeviceId();
    if (archive.header.contentsSize != 0 && isFromAnotherConsole)
        throw DataError(DataResult::FromAnotherConsole, "another console copied the channel");

    if (!HasRoomFor(MeasureNandRoom(services.fileSystem), RoomNeededFor(archive)))
        throw DataError(DataResult::NotEnoughRoom, "the NAND is full");

    Install(services, archive);
    return DataResult::Done;
}

}

Ios::SdPath ChannelBackupPath(std::uint32_t lowPart)
{
    return MakeSdPath(std::format("/private/wii/title/{}/{}", BackupFolderName(lowPart), ChannelBackupFileName));
}

DataResult CopyChannelToSdCard(DataManagementServices& services, Ios::TitleId titleId)
{
    try
    {
        return CopyToCard(services, titleId);
    }
    catch (const DataError& error)
    {
        return error.Result();
    }
}

DataResult CopyChannelToNand(DataManagementServices& services, std::uint32_t lowPart)
{
    try
    {
        return CopyToNand(services, lowPart);
    }
    catch (const DataError& error)
    {
        return error.Result();
    }
}

std::optional<ChannelOnCard> ReadChannelOnCard(DataManagementServices& services, std::uint32_t lowPart)
{
    Ios::SdPath file = ChannelBackupPath(lowPart);
    std::optional<std::uint64_t> fileSize = SdFileSize(services.sdCard, file);
    if (!fileSize)
        return std::nullopt;

    try
    {
        std::size_t largestStart = Formats::ChannelBackupHeaderSize + Formats::BiggestChannelIconSize;
        Formats::Bytes backupStart = ReadSdFileStart(services.sdCard, file, largestStart);
        return ReadHeaderAndIcon(services, backupStart, *fileSize, lowPart);
    }
    catch (const DataError&)
    {
        return std::nullopt;
    }
}

}
