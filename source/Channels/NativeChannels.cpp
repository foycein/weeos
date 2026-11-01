#include "Channels/NativeChannels.h"

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

}

bool HasNativeProgram(Ios::TitleId titleId)
{
    return titleId == ForecastChannelTitleId || titleId == NewsChannelTitleId;
}

std::unique_ptr<Channel> MakeChannel(Ios::TitleId titleId, ChannelServices& services)
{
    if (titleId == ForecastChannelTitleId)
        return std::make_unique<WiiConnect24Channel>(services, ForecastFiles);

    if (titleId == NewsChannelTitleId)
        return std::make_unique<WiiConnect24Channel>(services, NewsFiles);

    return nullptr;
}

}
