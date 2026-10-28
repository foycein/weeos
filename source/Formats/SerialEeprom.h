#pragma once

#include "Formats/Bytes.h"
#include "Formats/ConsoleKeys.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Formats
{

constexpr std::size_t SerialEepromSize = 256;

// The SEEPROM of the console (the file seeprom.bin) is a small memory that boot1 and IOS can change.
// This reads only what makes up the identity of the console. Behind it the memory holds the counters that stop
// a downgrade of boot2, the generation counters of the NAND, the key of Korean consoles and a seed for random numbers.
// Each comment gives the offset.
struct SerialEeprom
{
    // The numbers in the name of the signer of the device certificate: "Root-CA00000001-MS00000002" is signed by
    // the manufacturing signer 2 below the certificate authority 1.
    std::uint32_t manufacturingSignerId;                   // 0x00
    std::uint32_t certificateAuthorityId;                  // 0x04

    // The same values that keys.txt lists, see ConsoleKeys.
    std::uint32_t ngKeyId;                                 // 0x08
    std::array<Byte, NgSignatureSize> ngSignature;         // 0x0C
};

// Throws a FormatError when the data does not have exactly 256 bytes.
SerialEeprom ReadSerialEeprom(ByteView bytes);

}
