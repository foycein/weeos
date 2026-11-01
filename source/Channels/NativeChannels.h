#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"

#include "Ios/Title/TitleId.h"

#include <memory>

namespace Channels
{

// The channels that have a program in WeeOS.
constexpr Ios::TitleId WiiShopChannelTitleId = {0x00010002, 0x48414241};
constexpr Ios::TitleId ForecastChannelTitleId = {0x00010002, 0x48414645};
constexpr Ios::TitleId NewsChannelTitleId = {0x00010002, 0x48414745};
constexpr Ios::TitleId InternetChannelTitleId = {0x00010001, 0x48414445};
constexpr Ios::TitleId PhotoChannelTitleId = {0x00010002, 0x48415941};

// True when WeeOS has a program for the title. The menu shows its message for a title that cannot start for the others.
bool HasNativeProgram(Ios::TitleId titleId);

// The program of the channel. Nothing when WeeOS has none for the title. The services must outlive the channel.
std::unique_ptr<Channel> MakeChannel(Ios::TitleId titleId, ChannelServices& services);

}
