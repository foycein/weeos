#pragma once

#include "Formats/Bytes.h"
#include "Formats/ConsoleKeys.h"

#include <array>
#include <filesystem>
#include <optional>

namespace Ios
{

// The files of the identity folder (IosFolders::identityFolder), as ConsoleIdentityService reads them.
inline constexpr const char* KeysFileName = "keys.txt";
inline constexpr const char* DeviceCertificateFileName = "device.cert";
inline constexpr const char* SerialEepromFileName = "seeprom.bin";

// Only a console that WeeOS made has this file: the certificate of WeeOS's own authority that signed its device certificate.
inline constexpr const char* AuthorityCertificateFileName = "authority.cert";

using MacAddress = std::array<Formats::Byte, Formats::MacAddressSize>;

// Makes the identity of a new console, as a factory gives every Wii its own: a new console id, a new key pair and a certificate chain
// of WeeOS's own. The chain is an authority certificate ("WeeOS", issued by "Root") and the device certificate it signs; the private
// key of the authority is thrown away. The keys that every Wii shares (the common key, the SD key) are the real ones, so that Nintendo's
// titles install and save data on an SD card can be read. What this means:
//  - Titles signed by Nintendo still verify: their chain goes up to Nintendo's root, which has nothing to do with this identity.
//  - This WeeOS reads the save data it copied to an SD card: the identity service trusts its own authority.
//  - A real Wii, and another WeeOS, refuse save data signed by this console: its certificate does not lead up to Nintendo's root.
// Writes keys.txt, device.cert, seeprom.bin and authority.cert into the folder, which is made when it is not there.
// Throws std::runtime_error when a file cannot be written.
void WriteNewIdentity(const std::filesystem::path& identityFolder);

// Writes the identity of a real console from the key block of its BootMii backup (keys.bin, or the last 1024 bytes of nand.bin): its
// keys come from the OTP memory, its SEEPROM is kept, and its device certificate is made from both, as the console makes it. The address
// of the network card is not in the key block; without one the console gets a new one.
// Throws a FormatError when the key block cannot be read, std::runtime_error when a file cannot be written.
void WriteIdentityFromKeyBlock(Formats::ByteView keyBlock, const std::optional<MacAddress>& macAddress,
    const std::filesystem::path& identityFolder);

}
