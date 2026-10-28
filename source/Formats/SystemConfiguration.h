#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

enum class SettingType : std::uint8_t
{
    BigArray = 1,
    SmallArray = 2,
    Byte = 3,
    Short = 4,
    Long = 5,
    LongLong = 6,
    Bool = 7,
};

struct Setting
{
    // For example "IPL.LNG" or "BT.SENS".
    std::string name;

    SettingType type;

    // The value of a setting that is a number: every type except the two arrays.
    std::uint64_t number;

    // The bytes of a setting that is an array.
    Bytes bytes;
};

// The system configuration (the SYSCONF file) holds the settings of the console, like the language,
// the aspect ratio and the nickname. Each setting has a name and a value.
struct SystemConfiguration
{
    std::vector<Setting> settings;

    // The value of the setting with this name, if it is a number.
    std::optional<std::uint64_t> FindNumber(std::string_view name) const;

    // The bytes of the setting with this name, if it is an array.
    // The bytes stay inside the configuration, so they are only valid while it exists.
    std::optional<ByteView> FindBytes(std::string_view name) const&;
    std::optional<ByteView> FindBytes(std::string_view name) const&& = delete;
};

SystemConfiguration ReadSystemConfiguration(ByteView bytes);

}
