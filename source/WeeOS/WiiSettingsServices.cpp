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

Platform::ScreenShape ShapeOf(Settings::AspectRatio aspectRatio)
{
    if (aspectRatio == Settings::AspectRatio::FourByThree)
        return Platform::ScreenShape::FourByThree;

    return Platform::ScreenShape::SixteenByNine;
}

}

WiiSettingsServices::WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, Input::VirtualRemote& virtualRemote,
    ProgramSound& programSound, Settings::PcClock pcClock, const std::filesystem::path& pcSettingsFile)
    : window(programWindow)
    , remote(virtualRemote)
    , sound(programSound)
    , systemSettings(iosServices.GetFileSystem(), SystemMenuCaller)
    , clock([this] { return systemSettings.GetClockBias(); }, std::move(pcClock))
    , formatter(iosServices.GetFileSystem(), iosServices.GetTitles(), iosServices.GetPower())
    , pcSettings(pcSettingsFile)
    , flowServices{systemSettings, clock, iosServices.GetNetworkConfig(), iosServices.GetSockets(), SystemMenu::SearchWithRadio(iosServices.GetWireless()), formatter,
          pcSettings, *this}
{
    ApplyPcSettings(pcSettings.Values());
    remote.SetScreenShape(PictureShape());
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

void WiiSettingsServices::UseDisplay(SystemMenu::Display& menuDisplay)
{
    display = &menuDisplay;
}

// Fullscreen first, so that a window that leaves fullscreen gets its size.
void WiiSettingsServices::ApplyPcSettings(const Settings::PcSettings& settings)
{
    if (window.IsFullscreen() != settings.isFullscreen)
        window.ToggleFullscreen();

    window.SetClientSize({settings.windowWidth, settings.windowHeight});

    std::vector<Input::MouseBinding> mouseMap = Settings::MouseMapOf(settings);
    remote.SetKeyMap(settings.keyMap);
    remote.SetMouseMap(mouseMap);
    sound.ApplyPcSettings(settings);
}

void WiiSettingsServices::ApplyPictureShape(Settings::AspectRatio aspectRatio)
{
    remote.SetScreenShape(ShapeOf(aspectRatio));
    if (display != nullptr)
        display->SetScreenShape(ShapeOf(aspectRatio));
}

std::vector<std::u16string> WiiSettingsServices::ListAudioDevices() const
{
    return Sound::ListPlaybackDeviceNames();
}

}
