#pragma once

#include "Ios/SdCard/SdCardKind.h"

#include <cstdint>
#include <filesystem>
#include <mutex>

namespace Ios
{

// What the card service and the files that are open on the card share. The mutex guards all of it.
struct SdCardState
{
    std::mutex mutex;

    bool isInserted = false;
    bool isWriteProtected = false;
    SdCardKind cardKind = SdCardKind::Standard;

    // The folder on the PC that is the card.
    std::filesystem::path cardFolder;

    // Counts the cards that were put into the slot. A file belongs to the card it was opened on and stops working when that card is gone.
    std::uint32_t cardNumber = 0;
};

}
