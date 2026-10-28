#pragma once

#include "Ios/Network/MacAddress.h"
#include "Ios/Result.h"

#include <filesystem>

namespace Ios
{

// Reads the address of the wireless card of the console from the file keys.txt in the folder, where the console dump keeps it.
// NotFound when the folder has no keys.txt, InvalidArgument when the file is not a keys.txt.
Result<MacAddress> ReadWirelessMacAddress(const std::filesystem::path& identityFolder);

}
