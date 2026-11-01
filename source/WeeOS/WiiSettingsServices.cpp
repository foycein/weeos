#include "WeeOS/WiiSettingsServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Sound/PlaybackDevices.h"

#include <utility>

namespace WeeOS
{

namespace
{

// The System Menu reads and writes the settings of the console as its own user.
constexpr Ios::Caller SystemMenuCaller = {Ios::SystemMenuUserId, Ios::SystemMenuGroupId};

}

WiiSettingsServices::WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, Input::VirtualRemote& virtualRemote,
    ProgramSound& programSound, Settings::PcClock pcClock, Settings::PcSettingsStore& pcSettingsStore)
    : window(programWindow)
    , remote(virtualRemote)
    , sound(programSound)
    , systemSettings(iosServices.GetFileSystem(), SystemMenuCaller)
    , clock([this] { return systemSettings.GetClockBias(); }, std::move(pcClock))
    , formatter(iosServices.GetFileSystem(), iosServices.GetTitles(), iosServices.GetPower())
    , flowServices{systemSettings, clock, iosServices.GetNetworkConfig(), SystemMenu::SearchWithRadio(iosServices.GetWireless()), formatter,
          pcSettingsStore, *this}
{
    ApplyControlsAndSound(pcSettingsStore.Values());
    KeepThePictureWide();
}

SystemMenu::SettingsFlowServices& WiiSettingsServices::FlowServices()
{
    return flowServices;
}

Settings::SoundMode WiiSettingsServices::SoundMode() const
{
    return systemSettings.GetSoundMode();
}

// A page of PC Settings changed something: the window too.
void WiiSettingsServices::ApplyPcSettings(const Settings::PcSettings& settings)
{
    if (window.IsFullscreen() != settings.isFullscreen)
        window.ToggleFullscreen();

    window.SetClientSize({settings.windowWidth, settings.windowHeight});
    ApplyControlsAndSound(settings);
}

void WiiSettingsServices::ApplyControlsAndSound(const Settings::PcSettings& settings)
{
    std::vector<Input::MouseBinding> mouseMap = Settings::MouseMapOf(settings);
    remote.SetKeyMap(settings.keyMap);
    remote.SetMouseMap(mouseMap);
    sound.ApplyPcSettings(settings);
}

void WiiSettingsServices::ApplyPictureShape(Settings::AspectRatio)
{
    systemSettings.SetAspectRatio(Settings::AspectRatio::SixteenByNine);
}

// The console of an imported NAND may have been set to 4:3. Its settings say 16:9 from the first start on, as the menu and the
// channels read them.
void WiiSettingsServices::KeepThePictureWide()
{
    if (systemSettings.GetAspectRatio() == Settings::AspectRatio::SixteenByNine)
        return;

    systemSettings.SetAspectRatio(Settings::AspectRatio::SixteenByNine);
    systemSettings.Save();
}

std::vector<std::u16string> WiiSettingsServices::ListAudioDevices() const
{
    return Sound::ListPlaybackDeviceNames();
}

}
