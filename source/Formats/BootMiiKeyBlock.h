#pragma once

#include "Formats/Bytes.h"
#include "Formats/OneTimeProgrammable.h"
#include "Formats/SerialEeprom.h"

#include <string>

namespace Formats
{

// The 1024 bytes that BootMii writes at the end of a NAND backup, and also into a file of their own (keys.bin).
// They start with a text of 256 bytes like "BackupMii v1, ConsoleID: 0a447b97". The OTP memory follows at 0x100
// and the SEEPROM at 0x200. The rest is zeros.
struct BootMiiKeyBlock
{
    std::string header;
    OneTimeProgrammable oneTimeProgrammable;
    SerialEeprom serialEeprom;
};

// Throws a FormatError when the data does not have exactly 1024 bytes.
BootMiiKeyBlock ReadBootMiiKeyBlock(ByteView bytes);

}
