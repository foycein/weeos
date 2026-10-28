#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/Sha1.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Formats
{

constexpr std::size_t EllipticCurvePrivateKeySize = 30;
constexpr std::size_t NandHmacKeySize = 20;
constexpr std::size_t NgSignatureSize = 60;
constexpr std::size_t MacAddressSize = 6;

// The keys and numbers that belong to one console, as the file keys.txt of a BootMii backup lists them.
// Most of them come from the OTP memory of the console, two from its SEEPROM (see OneTimeProgrammable and SerialEeprom).
// "NG" is the name Nintendo uses for the key pair that identifies a console, as in "NG id" and "NG key id".
struct ConsoleKeys
{
    // The SHA-1 hash that boot0 expects boot1 to have.
    Sha1Digest boot1Hash;

    // Protects the title keys in tickets. Every retail Wii has the same one.
    AesKey commonKey;

    // The number of this console (the NG id). The device certificate is named after it: "NG0a447b97".
    std::uint32_t consoleId;

    // The private key of the console. It signs save data. The public key is in the device certificate.
    std::array<Byte, EllipticCurvePrivateKeySize> ellipticCurvePrivateKey;

    // Signs the clusters of the NAND file system.
    std::array<Byte, NandHmacKeySize> nandHmacKey;

    // Encrypts the clusters of the NAND file system.
    AesKey nandAesKey;

    // Starts the random number generator of IOS.
    AesKey randomNumberKey;

    // The number of the key in the device certificate, and the signature of that certificate made at the factory.
    std::uint32_t ngKeyId;
    std::array<Byte, NgSignatureSize> ngSignature;

    // Encrypt and protect the header of save data that is copied to an SD card. Every Wii has the same ones.
    AesKey sdKey;
    AesBlock sdInitializationVector;
    AesBlock md5Blanker;

    // The address of the wireless network card of this console.
    std::array<Byte, MacAddressSize> macAddress;
};

// Reads the text of keys.txt: one line for each value, written as "name = hexadecimal digits".
// Throws a FormatError for a missing or unknown name, a value of the wrong size, or a line that is not of this shape.
ConsoleKeys ReadConsoleKeys(ByteView keysTextBytes);

}
