#pragma once

#include "SystemMenu/DataManagement/DataManagementServices.h"

#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace SystemMenu
{

// What Data Management lists, and how big it is (iplNandSDWorker.cpp). The lists keep the order of the title service and of the card.

// A channel is complete when every content that is not optional is on the NAND (0x81351BB0).
bool IsChannelComplete(const Ios::TitleService& titles, Ios::TitleId titleId);

// The complete channels of the kinds that Data Management shows (0x8135212C).
std::vector<Ios::TitleId> ListNandChannels(DataManagementServices& services);

// The titles whose data folder holds anything (0x8134ADF0).
std::vector<Ios::TitleId> ListNandSaveData(DataManagementServices& services);

// The codes of the folders of /private/wii/title on the card that hold a content.bin or a data.bin that is longer than its header
// (0x81352F9C). At most 2400.
std::vector<std::uint32_t> ListChannelsOnCard(DataManagementServices& services);
std::vector<std::uint32_t> ListSaveDataOnCard(DataManagementServices& services);

// The size of a channel: its own contents and its meta data, without the contents it shares (0x8134CD68).
std::uint64_t NandChannelBytes(const DataManagementServices& services, Ios::TitleId titleId);

// The size of save data: its data folder (0x8134CB08).
std::uint64_t NandSaveDataBytes(const DataManagementServices& services, Ios::TitleId titleId);

// The size of the file on the card. Nothing when it is not there.
std::optional<std::uint64_t> CardChannelBytes(const DataManagementServices& services, std::uint32_t lowPart);
std::optional<std::uint64_t> CardSaveDataBytes(const DataManagementServices& services, std::uint32_t lowPart);

}
