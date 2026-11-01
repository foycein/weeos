#include "SystemMenu/DataManagement/DataItems.h"

#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataLists.h"
#include "SystemMenu/DataManagement/NandRoom.h"
#include "SystemMenu/DataManagement/SaveBackup.h"

#include "Formats/BannerContent.h"
#include "Formats/FormatError.h"
#include "Formats/SaveBanner.h"
#include "Formats/TextureDecoder.h"
#include "Ios/Title/TitlePaths.h"

#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view IconFileName = "meta/icon.bin";
constexpr std::string_view SaveBannerFileName = "banner.bin";
constexpr std::uint16_t BannerContentIndex = 0;

void TakeChannelName(DataItem& item, const std::array<Formats::ChannelName, Formats::BannerLanguageCount>& names)
{
    const Formats::ChannelName& englishName = names[Formats::EnglishLanguageNumber];
    item.firstLine = englishName.firstLine;
    item.secondLine = englishName.secondLine;
}

void TakeSaveBanner(DataItem& item, Formats::ByteView bannerFile)
{
    Formats::SaveBanner banner = Formats::ReadSaveBanner(bannerFile);
    item.firstLine = banner.title;
    item.secondLine = banner.subtitle;
    item.mayNotBeCopied = (banner.flags & Formats::SaveBannerNoCopyFlag) != 0;
    item.saveIcon = Formats::DecodeTexture(
        Formats::TextureFormat::Rgb5A3, Formats::SaveBannerIconSize, Formats::SaveBannerIconSize, banner.icons.front(), std::nullopt);
    item.savePicture = Formats::DecodeTexture(
        Formats::TextureFormat::Rgb5A3, Formats::SaveBannerPictureWidth, Formats::SaveBannerPictureHeight, banner.picture, std::nullopt);
}

Formats::Bytes ReadBannerContentOf(DataManagementServices& services, Ios::TitleId titleId)
{
    std::vector<Ios::TicketView> views = Require(services.titles.ListTicketViews(titleId), "read the tickets");
    if (views.empty())
        throw DataError(DataResult::Failed, "the channel has no ticket");

    Ios::ContentStream stream = Require(services.titles.OpenContent(titleId, views.front(), BannerContentIndex), "open the banner");
    return stream.Read(stream.GetLength());
}

DataItem NandChannelItem(DataManagementServices& services, Ios::TitleId titleId)
{
    DataItem item;
    item.lowPart = titleId.lowPart;
    item.titleId = titleId;
    item.blocks = BlocksOfBytes(NandChannelBytes(services, titleId));
    try
    {
        Formats::Bytes content = ReadBannerContentOf(services, titleId);
        Formats::BannerContent banner = Formats::ReadBannerContent(content);
        TakeChannelName(item, banner.names);

        std::optional<Formats::ByteView> icon = banner.files.FindFile(IconFileName);
        if (icon)
            item.channelIcon = Formats::Bytes(icon->begin(), icon->end());
    }
    catch (const DataError&)
    {
        item.isDamaged = true;
    }
    catch (const Formats::FormatError&)
    {
        item.isDamaged = true;
    }
    return item;
}

DataItem CardChannelItem(DataManagementServices& services, std::uint32_t lowPart)
{
    DataItem item;
    item.lowPart = lowPart;
    item.blocks = BlocksOfBytes(CardChannelBytes(services, lowPart).value_or(0));

    std::optional<ChannelOnCard> channel = ReadChannelOnCard(services, lowPart);
    if (!channel)
    {
        item.isDamaged = true;
        return item;
    }

    item.titleId = Ios::TitleId::FromValue(channel->header.titleId);
    item.channelIcon = channel->icon;
    try
    {
        TakeChannelName(item, Formats::ReadChannelNames(channel->header.bannerHeader));
    }
    catch (const Formats::FormatError&)
    {
        item.isDamaged = true;
    }
    return item;
}

Formats::Bytes ReadSaveBannerAsTitle(DataManagementServices& services, Ios::TitleId titleId)
{
    Require(services.titles.BecomeTitle(titleId), "become the title");
    NandAccess titleFiles(services.fileSystem, services.titles.GetCaller());
    try
    {
        Formats::Bytes banner = titleFiles.ReadFile(ChildOf(Ios::DataFolderPath(titleId), SaveBannerFileName));
        BecomeSystemMenuAgain(services.titles);
        return banner;
    }
    catch (const DataError&)
    {
        BecomeSystemMenuAgain(services.titles);
        throw;
    }
}

DataItem NandSaveItem(DataManagementServices& services, Ios::TitleId titleId)
{
    DataItem item;
    item.lowPart = titleId.lowPart;
    item.titleId = titleId;
    item.blocks = BlocksOfBytes(NandSaveDataBytes(services, titleId));
    try
    {
        TakeSaveBanner(item, ReadSaveBannerAsTitle(services, titleId));
    }
    catch (const DataError&)
    {
        item.isDamaged = true;
    }
    catch (const Formats::FormatError&)
    {
        item.isDamaged = true;
    }
    return item;
}

DataItem CardSaveItem(DataManagementServices& services, std::uint32_t lowPart)
{
    DataItem item;
    item.lowPart = lowPart;
    item.blocks = BlocksOfBytes(CardSaveDataBytes(services, lowPart).value_or(0));

    std::optional<Formats::SaveBackupHeader> header = ReadSaveBackupHeaderOnCard(services, lowPart);
    if (!header)
    {
        item.isDamaged = true;
        return item;
    }

    item.titleId = Ios::TitleId::FromValue(header->titleId);
    TakeSaveBanner(item, header->banner);
    return item;
}

}

std::vector<DataItem> ReadDataItems(DataManagementServices& services, DataKind kind, DataPlace place)
{
    std::vector<DataItem> items;
    bool isOnWii = place == DataPlace::Wii;

    if (kind == DataKind::Channels && isOnWii)
    {
        for (Ios::TitleId titleId : ListNandChannels(services))
            items.push_back(NandChannelItem(services, titleId));
    }
    else if (kind == DataKind::Channels)
    {
        for (std::uint32_t lowPart : ListChannelsOnCard(services))
            items.push_back(CardChannelItem(services, lowPart));
    }
    else if (isOnWii)
    {
        for (Ios::TitleId titleId : ListNandSaveData(services))
            items.push_back(NandSaveItem(services, titleId));
    }
    else
    {
        for (std::uint32_t lowPart : ListSaveDataOnCard(services))
            items.push_back(CardSaveItem(services, lowPart));
    }
    return items;
}

}
