#include "IdentityImporter.h"

#include "FolderFiles.h"

#include "Formats/ConsoleKeys.h"
#include "Formats/DeviceCertificate.h"
#include "Formats/FileBytes.h"
#include "Formats/SerialEeprom.h"

#include <format>
#include <stdexcept>

namespace
{

constexpr const char* KeysFileName = "keys.txt";
constexpr const char* DeviceCertificateFileName = "device.cert";
constexpr const char* SerialEepromFileName = "seeprom.bin";

}

void WriteIdentity(const std::filesystem::path& dumpFolder, const std::filesystem::path& targetFolder)
{
    Formats::Bytes keysBytes = Formats::ReadFileBytes(dumpFolder / KeysFileName);
    Formats::Bytes certificateBytes = Formats::ReadFileBytes(dumpFolder / DeviceCertificateFileName);
    Formats::Bytes serialEepromBytes = Formats::ReadFileBytes(dumpFolder / SerialEepromFileName);

    Formats::ConsoleKeys keys = Formats::ReadConsoleKeys(keysBytes);
    Formats::DeviceCertificate certificate = Formats::ReadDeviceCertificate(certificateBytes);
    Formats::ReadSerialEeprom(serialEepromBytes);

    if (certificate.consoleId != keys.consoleId)
    {
        throw std::runtime_error(std::format("The device certificate is for the console {:08X}, but keys.txt is for the console {:08X}",
            certificate.consoleId, keys.consoleId));
    }

    PrepareEmptyFolder(targetFolder);
    WriteBytesToFile(targetFolder / KeysFileName, keysBytes);
    WriteBytesToFile(targetFolder / DeviceCertificateFileName, certificateBytes);
    WriteBytesToFile(targetFolder / SerialEepromFileName, serialEepromBytes);
}
