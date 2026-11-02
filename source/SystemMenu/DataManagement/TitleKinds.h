#pragma once

#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace SystemMenu
{

// The kinds of titles that Data Management shows (iplNandSDWorker.cpp, 0x81349294 and 0x8134924C).
//
// A channel that can be copied is a title of the families 00010000 (discs), 00010001 (downloaded channels), 00010004 (channels of
// discs), 00010006 and 00010007. Save data belongs to the same families. The system channels (00010002), the hidden ones (00010008),
// the downloadable content (00010005) and the system itself never show.
//
// The low half of these titles is a code of four letters or digits, for example "HADE". A low half that starts with another printable
// character belongs to the data of a game; WeeOS has no games and leaves such titles out.
bool IsChannelKind(Ios::TitleId titleId);
bool HasSaveDataKind(Ios::TitleId titleId);

// The folder of a title on the SD card: /private/wii/title/<the four characters of the low half>.
std::string BackupFolderName(std::uint32_t lowPart);

// The low half of the title of a folder on the SD card. Nothing when the name is not four characters long.
std::optional<std::uint32_t> LowPartOfBackupFolder(std::string_view folderName);

}
