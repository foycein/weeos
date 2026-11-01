#pragma once

#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DataManagement/DataResult.h"
#include "SystemMenu/DataManagement/SaveBackup.h"

#include <cstdint>

namespace SystemMenu
{

// Erasing and moving in Data Management (iplNandSDWorker.cpp).

// Erases the contents and the meta data of a channel. The ticket, the title metadata and the save data stay, so the channel can come
// back from the SD card (0x8134FEA8).
DataResult EraseChannelFromNand(DataManagementServices& services, Ios::TitleId titleId);

// Erases the data folder of a title, banner.bin first (0x813533E0).
DataResult EraseSaveDataFromNand(DataManagementServices& services, Ios::TitleId titleId);

// Erases the file on the card. The folder stays.
DataResult EraseChannelFromCard(DataManagementServices& services, std::uint32_t lowPart);
DataResult EraseSaveDataFromCard(DataManagementServices& services, std::uint32_t lowPart);

// A move is a copy, and when the copy worked the original is erased (0x813543B8, 0x813544AC, 0x81354460, 0x8135460C).
DataResult MoveChannelToSdCard(DataManagementServices& services, Ios::TitleId titleId);
DataResult MoveChannelToNand(DataManagementServices& services, std::uint32_t lowPart);
SaveCopyReport MoveSaveDataToSdCard(DataManagementServices& services, Ios::TitleId titleId);

// The file on the card stays when some of its files could not go to the NAND.
SaveRestoreReport MoveSaveDataToNand(DataManagementServices& services, std::uint32_t lowPart);

}
