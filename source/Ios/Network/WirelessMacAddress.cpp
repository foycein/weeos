#include "Ios/Network/WirelessMacAddress.h"

#include "Formats/ConsoleKeys.h"
#include "Formats/FileBytes.h"
#include "Ios/Identity/IdentityFiles.h"

#include <stdexcept>
#include <system_error>

namespace Ios
{

Result<MacAddress> ReadWirelessMacAddress(const std::filesystem::path& identityFolder)
{
    std::filesystem::path keysFile = identityFolder / KeysFileName;
    std::error_code error;
    if (!std::filesystem::is_regular_file(keysFile, error))
        return IosError::NotFound;

    // The reader of the keys throws a FormatError, which is a runtime_error, for a file that is not a keys.txt.
    try
    {
        Formats::ConsoleKeys keys = Formats::ReadConsoleKeys(Formats::ReadFileBytes(keysFile));
        return keys.macAddress;
    }
    catch (const std::runtime_error&)
    {
        return IosError::InvalidArgument;
    }
}

}
