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

}

WiiSettingsServices::WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, ProgramSound& programSound,
    const Platform::RegionalSettings& regionalSettings, Settings::PcSettingsStore& pcSettingsStore)
    : window(programWindow)
    , remotes(iosServices.GetRemote())
    , sound(programSound)
    , systemSettings(iosServices.GetFileSystem(), SystemMenuCaller)
    , flowServices{systemSettings, iosServices.GetNetworkConfig(), pcSettingsStore, *this}
{
    TakeTheCountryOfThePc(regionalSettings);
    ApplyControlsAndSound(pcSettingsStore.Values());
    ApplyRumble(systemSettings.IsRumbleOn());
}

SystemMenu::SettingsFlowServices& WiiSettingsServices::FlowServices()
{
    return flowServices;
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
    ApplyControlsAndSound(settings);
}

void WiiSettingsServices::ApplyControlsAndSound(const Settings::PcSettings& settings)
{
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
