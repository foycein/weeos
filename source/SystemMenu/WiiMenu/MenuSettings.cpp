#include "SystemMenu/WiiMenu/MenuSettings.h"

#include "Formats/SystemConfiguration.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view LanguageSetting = "IPL.LNG";
constexpr std::string_view AspectRatioSetting = "IPL.AR";
constexpr std::string_view SensorBarSetting = "BT.BAR";

constexpr std::uint64_t WideScreenAspectRatio = 1;
constexpr std::uint64_t SensorBarAboveScreen = 1;

int MenuLanguageOf(std::uint64_t languageNumber)
{
    for (int language : LanguagesOfTheUsa)
    {
        if (static_cast<std::uint64_t>(language) == languageNumber)
            return language;
    }
    return LanguagesOfTheUsa.front();
}

}

MenuSettings ReadMenuSettings(MenuConsole& console)
{
    MenuSettings settings;

    std::optional<Formats::Bytes> file = console.ReadNandFile(SystemConfigurationPath);
    if (!file)
        return settings;

    Formats::SystemConfiguration configuration = Formats::ReadSystemConfiguration(*file);

    if (std::optional<std::uint64_t> language = configuration.FindNumber(LanguageSetting))
        settings.languageNumber = MenuLanguageOf(*language);

    if (std::optional<std::uint64_t> aspectRatio = configuration.FindNumber(AspectRatioSetting))
        settings.isWideScreen = *aspectRatio == WideScreenAspectRatio;

    if (std::optional<std::uint64_t> sensorBar = configuration.FindNumber(SensorBarSetting))
    {
        bool isAbove = *sensorBar == SensorBarAboveScreen;
        settings.sensorBarPosition = isAbove ? Input::SensorBarPosition::AboveScreen : Input::SensorBarPosition::BelowScreen;
    }

    return settings;
}

}
