#pragma once

#include "Formats/Bytes.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

struct SettingTextEntry
{
    std::string name;
    std::string value;
};

// The file setting.txt holds facts about the console, for example the serial number and the region.
// It is a text of lines like "CODE=LU", encrypted with a simple key.
struct SettingTextFile
{
    std::vector<SettingTextEntry> entries;

    std::optional<std::string> FindValue(std::string_view name) const;
};

SettingTextFile ReadSettingTextFile(ByteView encryptedBytes);

// The 256 bytes of the file: the lines, each ending with a carriage return and a line feed, encrypted, and zeros behind them as the
// console leaves them. Throws a FormatError when the text does not fit.
Bytes WriteSettingTextFile(const SettingTextFile& file);

}
