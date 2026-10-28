#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/ConsoleKeys.h"
#include "Formats/Sha1.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Formats
{

constexpr std::size_t OneTimeProgrammableSize = 128;

// The OTP memory of the console (the file otp.bin). It is burnt at the factory and cannot be changed again.
// The values are the same ones that keys.txt lists, see ConsoleKeys for what they mean. Each comment gives the offset.
// The last 8 bytes (0x78 to 0x7F) are not read: they vary between consoles and nobody knows what they are.
struct OneTimeProgrammable
{
    Sha1Digest boot1Hash;                                  // 0x00
    AesKey commonKey;                                      // 0x14
    std::uint32_t consoleId;                               // 0x24
    std::array<Byte, EllipticCurvePrivateKeySize> ellipticCurvePrivateKey;   // 0x28
    std::array<Byte, NandHmacKeySize> nandHmacKey;         // 0x44, its first two bytes are the last two bytes of the private key
    AesKey nandAesKey;                                     // 0x58
    AesKey randomNumberKey;                                // 0x68
};

// Throws a FormatError when the data does not have exactly 128 bytes.
OneTimeProgrammable ReadOneTimeProgrammable(ByteView bytes);

}
