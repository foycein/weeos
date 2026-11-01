#pragma once

#include "SystemMenu/DataManagement/DataManagementServices.h"

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

// The two editors of Data Management: channels (screen 0x0c) and the save data of the Wii (screen 0x0d).
enum class DataKind
{
    Channels,
    SaveData,
};

// Where the editor looks: the Wii System Memory (the NAND) or the SD card.
enum class DataPlace
{
    Wii,
    SdCard,
};

// One entry of the list of an editor, with what its slot and its window show.
struct DataItem
{
    // The low half of the title id: the code of four characters that names the folder on the SD card.
    std::uint32_t lowPart = 0;

    // The whole title id. Nothing for a file of the card that cannot be read.
    std::optional<Ios::TitleId> titleId;

    // The name of a channel in two lines, or the title and the subtitle of save data. Empty when the data is damaged.
    std::u16string firstLine;
    std::u16string secondLine;

    std::uint32_t blocks = 0;

    // The file on the card cannot be read: the menu shows a question mark.
    bool isDamaged = false;

    // A channel: its file meta/icon.bin, a layout of its own. Save data: the first icon of its banner, and the picture of its banner.
    Formats::Bytes channelIcon;
    std::optional<Formats::RgbaImage> saveIcon;
    std::optional<Formats::RgbaImage> savePicture;

    // The banner of the save data forbids copies.
    bool mayNotBeCopied = false;
};

// The entries in the order of the title service and of the card (iplNandSDWorker.cpp; the screens do not sort them).
std::vector<DataItem> ReadDataItems(DataManagementServices& services, DataKind kind, DataPlace place);

}
