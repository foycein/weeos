#pragma once

#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DataManagement/DataResult.h"

#include "Formats/SdCardBackup.h"
#include "Ios/SdCard/SdPath.h"

#include <cstdint>
#include <optional>

namespace SystemMenu
{

// Save data on the SD card: the file data.bin in /private/wii/title/<code> (iplNandSDWorker.cpp: 0x8134E150 copies to the card,
// 0x8134EDBC copies back to the NAND). The header holds the banner of the save data, the archive behind it the files.

Ios::SdPath SaveBackupPath(std::uint32_t lowPart);

struct SaveCopyReport
{
    DataResult result = DataResult::Done;

    // The save data has files in its directory "nocopy", which stayed on the NAND.
    bool hasNoCopyFiles = false;

    // The save data has files in its directory "notransfer", which go back only to this console.
    bool hasNoTransferFiles = false;
};

SaveCopyReport CopySaveDataToSdCard(DataManagementServices& services, Ios::TitleId titleId);

struct SaveRestoreReport
{
    DataResult result = DataResult::Done;

    // Files of the directory "notransfer" that belong to another console stayed on the card.
    bool hasLeftOutNoTransferFiles = false;
};

SaveRestoreReport CopySaveDataToNand(DataManagementServices& services, std::uint32_t lowPart);

// The decrypted header of a data.bin on the card. Nothing when the file is damaged or is not the file of this code.
std::optional<Formats::SaveBackupHeader> ReadSaveBackupHeaderOnCard(DataManagementServices& services, std::uint32_t lowPart);

}
