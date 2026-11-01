#include "SystemMenu/MenuSettings.h"

#include <stdexcept>

namespace SystemMenu
{

namespace
{

constexpr std::string_view SystemConfigurationPath = "/shared2/sys/SYSCONF";
constexpr std::string_view SetupDoneSettingName = "IPL.CD2";
constexpr std::string_view UpdateTypeSettingName = "IPL.UPT";
constexpr std::string_view ParentalControlSettingName = "IPL.PC";
constexpr std::string_view ContentRestrictionsSettingName = "NET.CTPC";
constexpr std::string_view InternetRestrictionSettingName = "WWW.RST";
constexpr std::uint64_t NewsRestrictionBit = 1;

// The parental controls are 74 bytes: the flag that says they are on, the organization that rates the games, the highest rating
// that is allowed, the PIN, the number of the secret question, the answer and the number of its characters.
constexpr std::size_t ParentalControlSize = 74;
constexpr std::size_t OnFlagOffset = 0;
constexpr std::size_t PinOffset = 3;
constexpr std::size_t PinLength = 4;

}

MenuSettings::MenuSettings(NandFiles& nandFiles)
    : files(nandFiles)
{
}

bool MenuSettings::IsConsoleSetUp()
{
    std::optional<Formats::SystemConfiguration> configuration = ReadConfiguration();
    if (!configuration)
        return false;

    return configuration->FindNumber(SetupDoneSettingName).value_or(0) != 0;
}

int MenuSettings::UpdateType()
{
    std::optional<Formats::SystemConfiguration> configuration = ReadConfiguration();
    if (!configuration)
        return 0;

    return static_cast<int>(configuration->FindNumber(UpdateTypeSettingName).value_or(0));
}

std::optional<ParentalControl> MenuSettings::ReadParentalControl()
{
    std::optional<Formats::SystemConfiguration> configuration = ReadConfiguration();
    if (!configuration)
        return std::nullopt;

    std::optional<Formats::ByteView> bytes = configuration->FindBytes(ParentalControlSettingName);
    if (!bytes || bytes->size() != ParentalControlSize)
        return std::nullopt;

    ParentalControl parentalControl;
    parentalControl.isOn = (*bytes)[OnFlagOffset] != 0;
    for (std::size_t index = 0; index < PinLength; ++index)
    {
        char character = static_cast<char>((*bytes)[PinOffset + index]);
        if (character == '\0')
            break;

        parentalControl.pin.push_back(character);
    }
    return parentalControl;
}

bool MenuSettings::IsNewsChannelRestricted()
{
    std::optional<Formats::SystemConfiguration> configuration = ReadConfiguration();
    if (!configuration)
        return false;

    return (configuration->FindNumber(ContentRestrictionsSettingName).value_or(0) & NewsRestrictionBit) != 0;
}

bool MenuSettings::IsInternetChannelRestricted()
{
    std::optional<Formats::SystemConfiguration> configuration = ReadConfiguration();
    if (!configuration)
        return false;

    return configuration->FindNumber(InternetRestrictionSettingName).value_or(0) != 0;
}

std::optional<Formats::SystemConfiguration> MenuSettings::ReadConfiguration()
{
    std::optional<Formats::Bytes> bytes = files.ReadFile(SystemConfigurationPath);
    if (!bytes)
        return std::nullopt;

    try
    {
        return Formats::ReadSystemConfiguration(*bytes);
    }
    catch (const std::runtime_error&)
    {
        return std::nullopt;
    }
}

}
