#pragma once

#include "Formats/Bytes.h"
#include "Formats/ConsoleKeys.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Formats
{

constexpr std::size_t SerialEepromSize = 256;
constexpr std::size_t KoreanKeySize = 16;

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

    // The version of boot2 that is installed. The memory keeps it twice, as the first byte of a record of ten bytes at 0x48 and at 0x52.
    // The second record is the version before the last update of boot2, so this is the first one.
    std::uint8_t bootTwoVersion;                           // 0x48

    // The key of Korean consoles. It is all zero in the consoles of every other region.
    std::array<Byte, KoreanKeySize> koreanKey;             // 0x74
};

// Throws a FormatError when the data does not have exactly 256 bytes.
SerialEeprom ReadSerialEeprom(ByteView bytes);

}
