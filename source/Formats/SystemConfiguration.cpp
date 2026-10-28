#include "Formats/SystemConfiguration.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <array>
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

// What the writer needs to know about the layout of the file.
constexpr std::size_t OffsetSize = sizeof(std::uint16_t);
constexpr std::size_t FirstOffsetPosition = TagSize + sizeof(std::uint16_t);
constexpr std::size_t MaximumNameLength = 32;
constexpr std::size_t MaximumSmallArrayLength = 256;
constexpr std::size_t MaximumBigArrayLength = 65536;
constexpr std::size_t SmallArrayLengthSize = 1;
constexpr std::size_t BigArrayLengthSize = 2;
constexpr char FirstNameCharacter = ' ';
constexpr char LastNameCharacter = '~';

// The lookup table has one slot for each setting that the System Menu knows, in this order (WiiBrew, the page /shared2/sys/SYSCONF).
// A slot holds the position of the offset of its setting in the offset list, or zero when the file does not have the setting.
// Some names are only guessed on that page, because no file had them. Their slots are kept.
constexpr std::size_t LookupSlotCount = 39;
constexpr std::array<std::string_view, LookupSlotCount> LookupSlotNames = {
    "IPL.APD", "MPLS.MOVIE", "IPL.TID", "WWW.RST", "DVD.CNF", "BT.BAR", "BT.MOT", "BT.SPKV", "BT.SENS", "BT.CDIF",
    "BT.DINF", "DEV.DSM", "DEV.CTC", "DEV.VIM", "DEV.BTM", "NET.WCFG", "NET.WCPC", "NET.PROF", "NET.CTPC", "NET.CNF",
    "IPL.UPT", "IPL.SND", "IPL.SADR", "IPL.SSV", "IPL.PGS", "IPL.PC", "IPL.NIK", "IPL.LNG", "IPL.INC", "IPL.IDL",
    "IPL.FRC", "IPL.EULA", "IPL.E60", "IPL.DH", "IPL.CD2", "IPL.CD", "IPL.ARN", "IPL.AR", "IPL.CB",
};
static_assert(LookupTableOffset + LookupSlotCount * OffsetSize == FooterOffset, "the lookup table ends where the footer starts");

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

namespace
{

void AppendNumber(Bytes& bytes, std::uint64_t number, std::size_t byteCount)
{
    for (std::size_t byteIndex = byteCount; byteIndex > 0; --byteIndex)
        bytes.push_back(static_cast<Byte>(number >> ((byteIndex - 1) * BitsPerByte)));
}

[[noreturn]] void FailToStore(const Setting& setting, const std::string& problem)
{
    throw FormatError(std::format("{}: the setting {} cannot be stored, {}", DataDescription, setting.name, problem));
}

void RequireStorableName(const Setting& setting)
{
    if (setting.name.empty() || setting.name.size() > MaximumNameLength)
        FailToStore(setting, std::format("its name has {} characters, but a name has 1 to {}", setting.name.size(), MaximumNameLength));

    for (char character : setting.name)
    {
        if (character < FirstNameCharacter || character > LastNameCharacter)
            FailToStore(setting, "its name has a character that is not plain ASCII");
    }
}

Bytes EncodeNumber(const Setting& setting, std::size_t byteCount)
{
    bool fits = byteCount >= sizeof(setting.number) || (setting.number >> (byteCount * BitsPerByte)) == 0;
    if (!fits)
        FailToStore(setting, std::format("the number {} does not fit in {} bytes", setting.number, byteCount));

    Bytes bytes;
    AppendNumber(bytes, setting.number, byteCount);
    return bytes;
}

// An array starts with its length minus one, in one byte (small array) or two bytes (big array).
Bytes EncodeArray(const Setting& setting, std::size_t maximumLength, std::size_t lengthSize)
{
    if (setting.bytes.empty() || setting.bytes.size() > maximumLength)
        FailToStore(setting, std::format("the array has {} bytes, but an array of this type has 1 to {}", setting.bytes.size(), maximumLength));

    Bytes bytes;
    AppendNumber(bytes, setting.bytes.size() - 1, lengthSize);
    bytes.insert(bytes.end(), setting.bytes.begin(), setting.bytes.end());
    return bytes;
}

Bytes EncodeValue(const Setting& setting)
{
    switch (setting.type)
    {
    case SettingType::BigArray:
        return EncodeArray(setting, MaximumBigArrayLength, BigArrayLengthSize);

    case SettingType::SmallArray:
        return EncodeArray(setting, MaximumSmallArrayLength, SmallArrayLengthSize);

    case SettingType::Byte:
    case SettingType::Bool:
        return EncodeNumber(setting, ByteSize);

    case SettingType::Short:
        return EncodeNumber(setting, ShortSize);

    case SettingType::Long:
        return EncodeNumber(setting, LongSize);

    case SettingType::LongLong:
        return EncodeNumber(setting, LongLongSize);
    }

    FailToStore(setting, "it has no known type");
}

Bytes EncodeSetting(const Setting& setting)
{
    RequireStorableName(setting);

    Byte typeBits = static_cast<Byte>(static_cast<Byte>(setting.type) << TypeShift);
    Byte nameLengthBits = static_cast<Byte>(setting.name.size() - 1);

    Bytes bytes;
    bytes.push_back(typeBits | nameLengthBits);
    bytes.insert(bytes.end(), setting.name.begin(), setting.name.end());

    Bytes value = EncodeValue(setting);
    bytes.insert(bytes.end(), value.begin(), value.end());
    return bytes;
}

// The position of the offset of the first setting with this name, or zero when there is none.
std::size_t OffsetPositionOf(const SystemConfiguration& configuration, std::string_view name)
{
    for (std::size_t settingNumber = 0; settingNumber < configuration.settings.size(); ++settingNumber)
    {
        if (configuration.settings[settingNumber].name == name)
            return FirstOffsetPosition + OffsetSize * settingNumber;
    }

    return 0;
}

}

Bytes WriteSystemConfiguration(const SystemConfiguration& configuration)
{
    std::vector<Bytes> encodedSettings;
    std::size_t endOffset = FirstOffsetPosition + OffsetSize * (configuration.settings.size() + 1);
    for (const Setting& setting : configuration.settings)
    {
        encodedSettings.push_back(EncodeSetting(setting));
        endOffset += encodedSettings.back().size();
    }

    if (endOffset > LookupTableOffset)
    {
        throw FormatError(std::format("{}: the settings need {:#x} bytes, but only {:#x} bytes are before the lookup table",
            DataDescription, endOffset, LookupTableOffset));
    }

    Bytes file(HeaderTag.begin(), HeaderTag.end());
    AppendNumber(file, configuration.settings.size(), sizeof(std::uint16_t));

    std::size_t offset = FirstOffsetPosition + OffsetSize * (configuration.settings.size() + 1);
    for (const Bytes& encodedSetting : encodedSettings)
    {
        AppendNumber(file, offset, OffsetSize);
        offset += encodedSetting.size();
    }
    AppendNumber(file, offset, OffsetSize);

    for (const Bytes& encodedSetting : encodedSettings)
        file.insert(file.end(), encodedSetting.begin(), encodedSetting.end());

    file.resize(LookupTableOffset, 0);
    for (std::string_view slotName : LookupSlotNames)
        AppendNumber(file, OffsetPositionOf(configuration, slotName), OffsetSize);

    file.insert(file.end(), FooterTag.begin(), FooterTag.end());
    return file;
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
