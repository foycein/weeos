#include "Settings/SystemSettings.h"

#include "Settings/SettingsError.h"

#include "Formats/FormatError.h"
#include "Ios/FileSystem/NewEntryDefaults.h"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

namespace Settings
{

namespace
{

constexpr std::string_view SettingsFilePath = "/shared2/sys/SYSCONF";

// The names of the settings in the file.
constexpr std::string_view AspectRatioName = "IPL.AR";
constexpr std::string_view LocationName = "IPL.SADR";
constexpr std::string_view NicknameName = "IPL.NIK";
constexpr std::string_view PointerSensitivityName = "BT.SENS";
constexpr std::string_view RumbleName = "BT.MOT";
constexpr std::string_view SpeakerVolumeName = "BT.SPKV";
constexpr std::uint8_t HighestSpeakerVolume = 127;
constexpr std::string_view SensorBarPositionName = "BT.BAR";
constexpr std::string_view SoundModeName = "IPL.SND";

// A setting that is a flag is on when it holds 1, and off when it holds any other number.
constexpr std::uint64_t FlagIsOn = 1;

// The remotes rumble when the file has no setting for it.
constexpr bool RumbleIsOnWithoutSetting = true;

constexpr int DefaultPointerSensitivity = 2;
constexpr int LowestPointerSensitivity = 1;
constexpr int HighestPointerSensitivity = 5;

// The nickname is 22 bytes: ten characters of 2 bytes and a number with the length of the name.
constexpr std::size_t NicknameSize = 22;
constexpr std::size_t NicknameCharacterCount = 10;
constexpr std::size_t NicknameLengthOffset = 20;

// The location is 4104 bytes. The first one is the country, the rest is the region, the city, their names and the coordinates.
constexpr std::size_t LocationSize = 4104;
constexpr std::size_t CountryOffset = 0;

constexpr std::size_t BytesPerCharacter = 2;

[[noreturn]] void FailWith(const std::string& problem, Ios::IosError error)
{
    throw SettingsError(std::format("{} (result code {})", problem, Ios::OriginalResultCode(error)));
}

Ios::NandPath ParseSettingsFilePath()
{
    Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(SettingsFilePath);
    if (path.Failed())
        FailWith("The path of the settings file is not accepted", path.Error());

    return path.Value();
}

Formats::Bytes ReadSettingsFile(Ios::FileSystemService& fileSystem, const Ios::Caller& caller)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(ParseSettingsFilePath(), Ios::OpenMode::Read, caller);
    if (opened.Failed())
        FailWith("The settings file cannot be opened", opened.Error());

    Ios::OpenFile& file = opened.Value();
    Ios::Result<Ios::FileStatistics> statistics = file.GetStatistics();
    if (statistics.Failed())
        FailWith("The size of the settings file cannot be read", statistics.Error());

    Ios::Result<Formats::Bytes> bytes = file.Read(statistics.Value().length);
    if (bytes.Failed())
        FailWith("The settings file cannot be read", bytes.Error());

    return bytes.Value();
}

void MakeSettingsFile(Ios::FileSystemService& fileSystem, const Ios::NandPath& path, const Ios::Caller& caller)
{
    Ios::Result<void> created = fileSystem.CreateFile(path, Ios::ReadWriteForEveryone, Ios::NoEntryFlags, caller);
    if (created.Failed())
        FailWith("The settings file cannot be made", created.Error());
}

void WriteSettingsFile(Ios::FileSystemService& fileSystem, const Ios::Caller& caller, Formats::ByteView bytes)
{
    Ios::NandPath path = ParseSettingsFilePath();

    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(path, Ios::OpenMode::Write, caller);
    if (opened.Failed() && opened.Error() == Ios::IosError::NotFound)
    {
        MakeSettingsFile(fileSystem, path, caller);
        opened = fileSystem.Open(path, Ios::OpenMode::Write, caller);
    }

    if (opened.Failed())
        FailWith("The settings file cannot be opened for writing", opened.Error());

    Ios::Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        FailWith("The settings file cannot be written", written.Error());

    if (written.Value() != bytes.size())
        throw SettingsError(std::format("The settings file was written with {} bytes of {}", written.Value(), bytes.size()));
}

Formats::SystemConfiguration ReadConfiguration(Ios::FileSystemService& fileSystem, const Ios::Caller& caller)
{
    Formats::Bytes bytes = ReadSettingsFile(fileSystem, caller);

    try
    {
        return Formats::ReadSystemConfiguration(bytes);
    }
    catch (const Formats::FormatError& error)
    {
        throw SettingsError(std::format("The settings file is not a settings file: {}", error.what()));
    }
}

bool FlagOf(const Formats::SystemConfiguration& configuration, std::string_view name, bool flagWithoutSetting)
{
    std::optional<std::uint64_t> number = configuration.FindNumber(name);
    if (!number)
        return flagWithoutSetting;

    return *number == FlagIsOn;
}

std::uint64_t FlagNumber(bool isOn)
{
    if (isOn)
        return FlagIsOn;

    return 0;
}

// The array of a setting, when it has the size that the console gives the setting.
std::optional<Formats::ByteView> ArrayOfSize(const Formats::SystemConfiguration& configuration, std::string_view name, std::size_t size)
{
    std::optional<Formats::ByteView> bytes = configuration.FindBytes(name);
    if (!bytes || bytes->size() != size)
        return std::nullopt;

    return bytes;
}

std::uint16_t ReadNumber16(Formats::ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((bytes[offset] << Formats::BitsPerByte) | bytes[offset + 1]);
}

void WriteNumber16(Formats::Bytes& bytes, std::size_t offset, std::uint16_t number)
{
    bytes[offset] = static_cast<Formats::Byte>(number >> Formats::BitsPerByte);
    bytes[offset + 1] = static_cast<Formats::Byte>(number & Formats::ByteMask);
}

// Text is kept as characters of two bytes, the first byte first.
std::u16string ReadText(Formats::ByteView bytes, std::size_t offset, std::size_t length, std::size_t characterCount)
{
    std::u16string text;
    std::size_t usedLength = std::min(length, characterCount);
    for (std::size_t characterNumber = 0; characterNumber < usedLength; ++characterNumber)
        text.push_back(static_cast<char16_t>(ReadNumber16(bytes, offset + characterNumber * BytesPerCharacter)));

    return text;
}

void WriteText(Formats::Bytes& bytes, std::size_t offset, const std::u16string& text, std::size_t characterCount)
{
    std::size_t usedLength = std::min(text.size(), characterCount);
    for (std::size_t characterNumber = 0; characterNumber < usedLength; ++characterNumber)
        WriteNumber16(bytes, offset + characterNumber * BytesPerCharacter, static_cast<std::uint16_t>(text[characterNumber]));
}

}

SystemSettings::SystemSettings(Ios::FileSystemService& fileSystemToUse, const Ios::Caller& menuCaller)
    : fileSystem(fileSystemToUse)
    , caller(menuCaller)
    , configuration(ReadConfiguration(fileSystemToUse, menuCaller))
{
}

void SystemSettings::Save()
{
    Formats::Bytes bytes;
    try
    {
        bytes = Formats::WriteSystemConfiguration(configuration);
    }
    catch (const Formats::FormatError& error)
    {
        throw SettingsError(std::format("The settings cannot be stored: {}", error.what()));
    }

    WriteSettingsFile(fileSystem, caller, bytes);
    hasUnsavedChanges = false;
}

bool SystemSettings::HasUnsavedChanges() const
{
    return hasUnsavedChanges;
}

std::uint8_t SystemSettings::GetCountryCode() const
{
    std::optional<Formats::ByteView> location = configuration.FindBytes(LocationName);
    if (!location || location->empty())
        return 0;

    return (*location)[CountryOffset];
}

void SystemSettings::SetCountryCode(std::uint8_t countryCode)
{
    Formats::Bytes location(LocationSize, 0);
    location[CountryOffset] = countryCode;
    StoreBytes(LocationName, Formats::SettingType::BigArray, std::move(location));
}

AspectRatio SystemSettings::GetAspectRatio() const
{
    if (FlagOf(configuration, AspectRatioName, false))
        return AspectRatio::SixteenByNine;

    return AspectRatio::FourByThree;
}

SoundMode SystemSettings::GetSoundMode() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(SoundModeName);
    if (!number || *number > static_cast<std::uint64_t>(SoundMode::Surround))
        return SoundMode::Stereo;

    return static_cast<SoundMode>(*number);
}

void SystemSettings::SetSoundMode(SoundMode soundMode)
{
    StoreNumber(SoundModeName, Formats::SettingType::Byte, static_cast<std::uint64_t>(soundMode));
}

SensorBarPosition SystemSettings::GetSensorBarPosition() const
{
    if (FlagOf(configuration, SensorBarPositionName, false))
        return SensorBarPosition::AboveTelevision;

    return SensorBarPosition::BelowTelevision;
}

void SystemSettings::SetSensorBarPosition(SensorBarPosition position)
{
    StoreNumber(SensorBarPositionName, Formats::SettingType::Byte, static_cast<std::uint64_t>(position));
}

int SystemSettings::GetPointerSensitivity() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(PointerSensitivityName);
    if (!number)
        return DefaultPointerSensitivity;

    int sensitivity = static_cast<int>(std::min<std::uint64_t>(*number, HighestPointerSensitivity));
    return std::max(sensitivity, LowestPointerSensitivity);
}

void SystemSettings::SetPointerSensitivity(int sensitivity)
{
    int storedSensitivity = std::clamp(sensitivity, LowestPointerSensitivity, HighestPointerSensitivity);
    StoreNumber(PointerSensitivityName, Formats::SettingType::Long, static_cast<std::uint64_t>(storedSensitivity));
}

std::u16string SystemSettings::GetNickname() const
{
    std::optional<Formats::ByteView> bytes = ArrayOfSize(configuration, NicknameName, NicknameSize);
    if (!bytes)
        return std::u16string(FreshConsoleNickname);

    std::u16string nickname = ReadText(*bytes, 0, ReadNumber16(*bytes, NicknameLengthOffset), NicknameCharacterCount);
    if (nickname.empty())
        return std::u16string(FreshConsoleNickname);

    return nickname;
}

void SystemSettings::SetNickname(const std::u16string& nickname)
{
    Formats::Bytes bytes(NicknameSize, 0);
    WriteText(bytes, 0, nickname, NicknameCharacterCount);
    WriteNumber16(bytes, NicknameLengthOffset, static_cast<std::uint16_t>(std::min(nickname.size(), NicknameCharacterCount)));
    StoreBytes(NicknameName, Formats::SettingType::SmallArray, std::move(bytes));
}

bool SystemSettings::IsRumbleOn() const
{
    return FlagOf(configuration, RumbleName, RumbleIsOnWithoutSetting);
}

void SystemSettings::SetRumble(bool isOn)
{
    StoreNumber(RumbleName, Formats::SettingType::Byte, FlagNumber(isOn));
}

std::uint8_t SystemSettings::GetSpeakerVolume() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(SpeakerVolumeName);
    if (!number)
        return DefaultSpeakerVolume;

    return static_cast<std::uint8_t>(std::min<std::uint64_t>(*number, HighestSpeakerVolume));
}

void SystemSettings::SetSpeakerVolume(std::uint8_t volume)
{
    std::uint8_t storedVolume = std::min(volume, HighestSpeakerVolume);
    StoreNumber(SpeakerVolumeName, Formats::SettingType::Byte, storedVolume);
}

const Formats::SystemConfiguration& SystemSettings::Configuration() const
{
    return configuration;
}

Formats::Setting& SystemSettings::FindOrAddSetting(std::string_view name)
{
    for (Formats::Setting& setting : configuration.settings)
    {
        if (setting.name == name)
            return setting;
    }

    configuration.settings.push_back(Formats::Setting{std::string(name), Formats::SettingType::Byte, 0, {}});
    return configuration.settings.back();
}

void SystemSettings::StoreNumber(std::string_view name, Formats::SettingType type, std::uint64_t number)
{
    Formats::Setting& setting = FindOrAddSetting(name);
    setting.type = type;
    setting.number = number;
    setting.bytes.clear();
    hasUnsavedChanges = true;
}

void SystemSettings::StoreBytes(std::string_view name, Formats::SettingType type, Formats::Bytes bytes)
{
    Formats::Setting& setting = FindOrAddSetting(name);
    setting.type = type;
    setting.number = 0;
    setting.bytes = std::move(bytes);
    hasUnsavedChanges = true;
}

}
