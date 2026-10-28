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

}
