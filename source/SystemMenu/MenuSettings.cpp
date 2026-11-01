#include "SystemMenu/MenuSettings.h"

#include <stdexcept>

namespace SystemMenu
{

namespace
{

constexpr std::string_view SystemConfigurationPath = "/shared2/sys/SYSCONF";
constexpr std::string_view SetupDoneSettingName = "IPL.CD2";
constexpr std::string_view UpdateTypeSettingName = "IPL.UPT";

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
