#include "Formats/SystemConfiguration.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "System configuration";
constexpr std::size_t FileSize = 0x4000;
constexpr std::size_t TagSize = 4;
constexpr std::string_view HeaderTag = "SCv0";
constexpr std::string_view FooterTag = "SCed";
constexpr std::size_t FooterOffset = FileSize - TagSize;

// The file ends with a table that lets the console find settings quickly. It lies between the
// settings and the footer. The settings are found without it.
constexpr std::size_t LookupTableOffset = 0x3FAE;

constexpr Byte TypeShift = 5;
constexpr Byte NameLengthMask = 0x1F;
constexpr Byte HighestSettingType = 7;
constexpr std::size_t ByteSize = 1;
constexpr std::size_t ShortSize = 2;
constexpr std::size_t LongSize = 4;
constexpr std::size_t LongLongSize = 8;

std::uint64_t ReadNumber(ByteReader& reader, std::size_t byteCount)
{
    std::uint64_t number = 0;
    for (std::size_t byteIndex = 0; byteIndex < byteCount; ++byteIndex)
        number = (number << BitsPerByte) | reader.ReadByte();
    return number;
}

SettingType ReadSettingType(ByteReader& reader, Byte typeNumber)
{
    if (typeNumber == 0 || typeNumber > HighestSettingType)
        reader.Fail(std::format("the setting type {} is not known", typeNumber));

    return static_cast<SettingType>(typeNumber);
}

void ReadSettingValue(ByteReader& reader, Setting& setting)
{
    setting.number = 0;

    switch (setting.type)
    {
    case SettingType::BigArray:
    {
        std::size_t length = std::size_t(reader.ReadUint16()) + 1;
        ByteView bytes = reader.ReadBytes(length);
        setting.bytes.assign(bytes.begin(), bytes.end());
        return;
    }

    case SettingType::SmallArray:
    {
        std::size_t length = std::size_t(reader.ReadByte()) + 1;
        ByteView bytes = reader.ReadBytes(length);
        setting.bytes.assign(bytes.begin(), bytes.end());
        return;
    }

    case SettingType::Byte:
    case SettingType::Bool:
        setting.number = ReadNumber(reader, ByteSize);
        return;

    case SettingType::Short:
        setting.number = ReadNumber(reader, ShortSize);
        return;

    case SettingType::Long:
        setting.number = ReadNumber(reader, LongSize);
        return;

    case SettingType::LongLong:
        setting.number = ReadNumber(reader, LongLongSize);
        return;
    }
}

// A setting starts with a byte that holds the type in its top three bits and the length of the name minus one
// in its low five bits. The name and the value follow.
Setting ReadSetting(ByteReader& reader)
{
    Byte typeAndNameLength = reader.ReadByte();

    Setting setting;
    setting.type = ReadSettingType(reader, typeAndNameLength >> TypeShift);
    setting.name = reader.ReadText(std::size_t(typeAndNameLength & NameLengthMask) + 1);
    ReadSettingValue(reader, setting);
    return setting;
}

}

SystemConfiguration ReadSystemConfiguration(ByteView bytes)
{
    ByteReader reader(bytes, DataDescription);
    if (bytes.size() != FileSize)
        reader.Fail(std::format("the file has {} bytes instead of {}", bytes.size(), FileSize));

    std::string header = reader.ReadText(TagSize);
    if (header != HeaderTag)
        reader.Fail(std::format("the header {} is not {}", header, HeaderTag));

    reader.MoveTo(FooterOffset);
    std::string footer = reader.ReadText(TagSize);
    if (footer != FooterTag)
        reader.Fail(std::format("the footer {} is not {}", footer, FooterTag));

    reader.MoveTo(TagSize);
    std::uint16_t settingCount = reader.ReadUint16();

    std::vector<std::uint16_t> offsets;
    for (std::uint32_t offsetNumber = 0; offsetNumber <= settingCount; ++offsetNumber)
        offsets.push_back(reader.ReadUint16());

    if (offsets.back() > LookupTableOffset)
        reader.Fail(std::format("the settings end at {:#x}, behind the start of the lookup table at {:#x}", offsets.back(), LookupTableOffset));

    SystemConfiguration configuration;
    for (std::size_t settingNumber = 0; settingNumber < settingCount; ++settingNumber)
    {
        reader.MoveTo(offsets[settingNumber]);
        configuration.settings.push_back(ReadSetting(reader));

        std::size_t nextOffset = offsets[settingNumber + 1];
        if (reader.Position() != nextOffset)
        {
            reader.Fail(std::format("the setting {} ends at {:#x}, but the next one starts at {:#x}",
                configuration.settings.back().name, reader.Position(), nextOffset));
        }
    }

    return configuration;
}

std::optional<std::uint64_t> SystemConfiguration::FindNumber(std::string_view name) const
{
    for (const Setting& setting : settings)
    {
        bool isArray = setting.type == SettingType::BigArray || setting.type == SettingType::SmallArray;
        if (setting.name == name && !isArray)
            return setting.number;
    }

    return std::nullopt;
}

std::optional<ByteView> SystemConfiguration::FindBytes(std::string_view name) const&
{
    for (const Setting& setting : settings)
    {
        bool isArray = setting.type == SettingType::BigArray || setting.type == SettingType::SmallArray;
        if (setting.name == name && isArray)
            return ByteView(setting.bytes);
    }

    return std::nullopt;
}

}
