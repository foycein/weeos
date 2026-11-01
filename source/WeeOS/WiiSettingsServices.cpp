#include "WeeOS/WiiSettingsServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Settings/Countries.h"
#include "Sound/PlaybackDevices.h"

namespace WeeOS
{

namespace
{

// The System Menu reads and writes the settings of the console as its own user.
constexpr Ios::Caller SystemMenuCaller = {Ios::SystemMenuUserId, Ios::SystemMenuGroupId};

Platform::ScreenShape ShapeOf(Settings::AspectRatio aspectRatio)
{
    if (aspectRatio == Settings::AspectRatio::FourByThree)
        return Platform::ScreenShape::FourByThree;

    return Platform::ScreenShape::SixteenByNine;
}

}

WiiSettingsServices::WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, ProgramSound& programSound,
    const Platform::RegionalSettings& regionalSettings, const std::filesystem::path& pcSettingsFile)
    : window(programWindow)
    , remotes(iosServices.GetRemote())
    , sound(programSound)
    , systemSettings(iosServices.GetFileSystem(), SystemMenuCaller)
    , pcSettings(pcSettingsFile)
    , flowServices{systemSettings, iosServices.GetNetworkConfig(), iosServices.GetSockets(), pcSettings, *this}
{
    TakeTheCountryOfThePc(regionalSettings);
    ApplyPcSettings(pcSettings.Values());
    ApplyRumble(systemSettings.IsRumbleOn());
    remotes.GetVirtualRemote().SetScreenShape(PictureShape());
}

SystemMenu::SettingsFlowServices& WiiSettingsServices::FlowServices()
{
    return flowServices;
}

Platform::ScreenShape WiiSettingsServices::PictureShape() const
{
    return ShapeOf(systemSettings.GetAspectRatio());
}

Settings::SoundMode WiiSettingsServices::SoundMode() const
{
    return systemSettings.GetSoundMode();
}

// Fullscreen first, so that a window that leaves fullscreen gets its size.
void WiiSettingsServices::ApplyPcSettings(const Settings::PcSettings& settings)
{
    if (window.IsFullscreen() != settings.isFullscreen)
        window.ToggleFullscreen();

    window.SetClientSize({settings.windowWidth, settings.windowHeight});

    Input::VirtualRemote& remote = remotes.GetVirtualRemote();
    remote.SetKeyMap(settings.keyMap);
    remote.SetMouseMap(Settings::MouseMapOf(settings));
    sound.ApplyPcSettings(settings);
}

void WiiSettingsServices::ApplyRumble(bool isRumbleOn)
{
    remotes.SetRumbleEnabled(isRumbleOn);
}

std::vector<std::u16string> WiiSettingsServices::ListAudioDevices() const
{
    return Sound::ListPlaybackDeviceNames();
}

// The console is where the PC is (docs/DECISIONS.md, Settings). The file is written only when the country changed.
void WiiSettingsServices::TakeTheCountryOfThePc(const Platform::RegionalSettings& regionalSettings)
{
    std::uint8_t countryCode = Settings::CountryCodeOfThePc(regionalSettings.countryLetters);
    if (systemSettings.GetCountryCode() == countryCode)
        return;

    systemSettings.SetCountryCode(countryCode);
    systemSettings.Save();
}

}
