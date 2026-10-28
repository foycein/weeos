#include "PersonalSettings.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace
{

// The nickname is 10 characters of 2 bytes and a word with the number of characters.
constexpr std::size_t NicknameSize = 22;

// The parental controls are: a flag that says they are on, the organization that rates the games (1 is the ESRB of the USA),
// the highest rating that is allowed (0x14 allows everything), the PIN (4 bytes), the number of the secret question,
// the answer (64 bytes) and a word with the number of characters of the answer.
constexpr std::size_t ParentalControlsSize = 74;
constexpr std::size_t OrganizationOffset = 1;
constexpr std::size_t HighestAllowedRatingOffset = 2;
constexpr Formats::Byte FactoryOrganization = 1;
constexpr Formats::Byte FactoryHighestAllowedRating = 0x14;

// The address starts with the code of the country. The rest is the region, the city, names and coordinates.
constexpr std::size_t CountryCodeSize = 1;

// The channel grid of the System Menu has 48 slots. One count says how many have a channel, the other how many are free.
// A console from the factory has the five channels that the import keeps: Photo, Mii, Wii Shop, Forecast and News.
// Assumed: the meaning of the two counts is read from their values on the dump's console (28 and 20, which add up to 48,
// and 28 is those five channels and 23 downloaded ones). See docs/audit/OpenPoints/Formats.md.
constexpr std::uint64_t ChannelGridSlotCount = 48;
constexpr std::uint64_t FactoryChannelCount = 5;

constexpr std::string_view NicknameSettingName = "IPL.NIK";
constexpr std::string_view ParentalControlsSettingName = "IPL.PC";
constexpr std::string_view SyncedWiiRemotesSettingName = "BT.DINF";
constexpr std::string_view GuestWiiRemotesSettingName = "BT.CDIF";
constexpr std::string_view AddressSettingName = "IPL.SADR";
constexpr std::string_view ClockBiasSettingName = "IPL.CB";
constexpr std::string_view TemporaryChannelSettingName = "IPL.TID";
constexpr std::string_view InstalledChannelCountSettingName = "IPL.INC";
constexpr std::string_view FreeChannelCountSettingName = "IPL.FRC";
constexpr std::string_view NetworkProfileSettingName = "NET.PROF";
constexpr std::string_view NetworkConfigurationSettingName = "NET.CNF";

Formats::Setting* FindSetting(Formats::SystemConfiguration& configuration, std::string_view name)
{
    for (Formats::Setting& setting : configuration.settings)
    {
        if (setting.name == name)
            return &setting;
    }

    return nullptr;
}

void ClearBytes(Formats::Setting& setting, std::size_t firstByteToClear)
{
    std::fill(setting.bytes.begin() + firstByteToClear, setting.bytes.end(), Formats::Byte(0));
}

void RequireSize(const Formats::Setting& setting, std::size_t expectedSize)
{
    if (setting.bytes.size() == expectedSize)
        return;

    throw Formats::FormatError(std::format("System configuration: {} has {} bytes instead of {}, so its personal values cannot be found",
        setting.name, setting.bytes.size(), expectedSize));
}

void ResetNickname(Formats::SystemConfiguration& configuration)
{
    Formats::Setting* nickname = FindSetting(configuration, NicknameSettingName);
    if (nickname == nullptr)
        return;

    RequireSize(*nickname, NicknameSize);
    ClearBytes(*nickname, 0);
}

void ResetParentalControls(Formats::SystemConfiguration& configuration)
{
    Formats::Setting* parentalControls = FindSetting(configuration, ParentalControlsSettingName);
    if (parentalControls == nullptr)
        return;

    RequireSize(*parentalControls, ParentalControlsSize);
    ClearBytes(*parentalControls, 0);
    parentalControls->bytes[OrganizationOffset] = FactoryOrganization;
    parentalControls->bytes[HighestAllowedRatingOffset] = FactoryHighestAllowedRating;
}

void ClearArray(Formats::SystemConfiguration& configuration, std::string_view name, std::size_t firstByteToClear)
{
    Formats::Setting* setting = FindSetting(configuration, name);
    if (setting != nullptr)
        ClearBytes(*setting, std::min(firstByteToClear, setting->bytes.size()));
}

void SetNumber(Formats::SystemConfiguration& configuration, std::string_view name, std::uint64_t number)
{
    Formats::Setting* setting = FindSetting(configuration, name);
    if (setting != nullptr)
        setting->number = number;
}

void RemoveSetting(Formats::SystemConfiguration& configuration, std::string_view name)
{
    std::erase_if(configuration.settings, [&](const Formats::Setting& setting) { return setting.name == name; });
}

}

void ResetPersonalSettings(Formats::SystemConfiguration& configuration)
{
    ResetNickname(configuration);
    ResetParentalControls(configuration);
    ClearArray(configuration, SyncedWiiRemotesSettingName, 0);
    ClearArray(configuration, GuestWiiRemotesSettingName, 0);
    ClearArray(configuration, AddressSettingName, CountryCodeSize);
    SetNumber(configuration, ClockBiasSettingName, 0);
    SetNumber(configuration, TemporaryChannelSettingName, 0);
    SetNumber(configuration, InstalledChannelCountSettingName, FactoryChannelCount);
    SetNumber(configuration, FreeChannelCountSettingName, ChannelGridSlotCount - FactoryChannelCount);
    RemoveSetting(configuration, NetworkProfileSettingName);
    RemoveSetting(configuration, NetworkConfigurationSettingName);
}

Formats::Bytes ResetPersonalSettingsOfFile(Formats::ByteView systemConfigurationBytes)
{
    Formats::SystemConfiguration configuration = Formats::ReadSystemConfiguration(systemConfigurationBytes);
    ResetPersonalSettings(configuration);
    return Formats::WriteSystemConfiguration(configuration);
}
