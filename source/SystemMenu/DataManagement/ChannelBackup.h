#pragma once

#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DataManagement/DataResult.h"

#include "Formats/SdCardBackup.h"
#include "Ios/SdCard/SdPath.h"

#include <cstdint>
#include <optional>

namespace SystemMenu
{

// Channels on the SD card: the file content.bin in /private/wii/title/<code> (iplNandSDWorker.cpp: 0x8134FFDC copies to the card,
// 0x813510F4 installs from the card). The header holds the names of the channel and its icon follows it; the archive behind them
// holds the title metadata and the contents. The ticket stays on the NAND, so a channel goes back only to a console that has it.

Ios::SdPath ChannelBackupPath(std::uint32_t lowPart);

// The console has slots for 48 channels.
constexpr std::size_t ChannelSlotCount = 48;

DataResult CopyChannelToSdCard(DataManagementServices& services, Ios::TitleId titleId);
DataResult CopyChannelToNand(DataManagementServices& services, std::uint32_t lowPart);

// A channel on the card, as the lists of Data Management and the SD Card Menu show it.
struct ChannelOnCard
{
    Formats::ChannelBackupHeader header;

    // The file meta/icon.bin of the banner of the channel.
    Formats::Bytes icon;
};

// Reads the header and the icon of a content.bin. Nothing when the file is damaged or is not the file of this code.
std::optional<ChannelOnCard> ReadChannelOnCard(DataManagementServices& services, std::uint32_t lowPart);

}
