#pragma once

#include "Formats/Aes128.h"

namespace Formats
{

// The common key of retail Wii consoles. Tickets keep their title key encrypted with it.
inline constexpr AesKey WiiCommonKey = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// The key that protects the header of save data on an SD card (sd_key of keys.txt). Every Wii has the same one, so that a save
// copied to an SD card can go to another Wii. The vector that goes with it is SdCardInitializationVector (SdCardBackup.h).
inline constexpr AesKey WiiSdKey = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

}
