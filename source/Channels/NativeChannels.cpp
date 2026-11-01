#include "Channels/NativeChannels.h"

#include "Channels/ShopChannel.h"
#include "Channels/WiiConnect24Channel.h"

namespace Channels
{

namespace
{

// The Forecast Channel 7 (HAFE): content 9 holds the layouts, content 6 is the shared HOME Button Menu.
constexpr WiiConnect24ChannelFiles ForecastFiles = {
    9, "./weather_layout.arc.LZ", "arc/font/font_weather.brfnt", "font_weather.brfnt", 8, 6};

// The News Channel 7 (HAGE): the same places, its own layouts and font, and one more message, the update the News Channel needs.
constexpr WiiConnect24ChannelFiles NewsFiles = {
    9, "./news_layout.arc.LZ", "arc/font/font_news.brfnt", "font_news.brfnt", 9, 6};

// The fonts and the HOME menu of each channel: the Forecast and the News open HomeButton3 (0x8019B978 and 0x801B2488 of their programs),
// the Shop HomeButton2 (0x802F75B8).
constexpr std::size_t WiiMenuAndReset = 2;
constexpr ChannelSharedFiles ForecastSharedFiles = {7, 6, "./HomeButton3", WiiMenuAndReset};
constexpr ChannelSharedFiles NewsSharedFiles = {7, 6, "./HomeButton3", WiiMenuAndReset};
constexpr ChannelSharedFiles ShopSharedFiles = {9, 7, "./HomeButton2", WiiMenuAndReset};

}

std::optional<ChannelSharedFiles> SharedFilesOf(Ios::TitleId titleId)
{
    if (titleId == WiiShopChannelTitleId)
        return ShopSharedFiles;

    if (titleId == ForecastChannelTitleId)
        return ForecastSharedFiles;

    if (titleId == NewsChannelTitleId)
        return NewsSharedFiles;

    return std::nullopt;
}

bool HasNativeProgram(Ios::TitleId titleId)
{
    return SharedFilesOf(titleId).has_value();
}

std::unique_ptr<Channel> MakeChannel(Ios::TitleId titleId, ChannelServices& services)
{
    if (titleId == WiiShopChannelTitleId)
        return std::make_unique<ShopChannel>(services);

    if (titleId == ForecastChannelTitleId)
        return std::make_unique<WiiConnect24Channel>(services, ForecastFiles);

    if (titleId == NewsChannelTitleId)
        return std::make_unique<WiiConnect24Channel>(services, NewsFiles);

    return nullptr;
}

}
