#include "WeeOS/WiiSettingsServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Platform/FileLocations.h"
#include "Sound/PlaybackDevices.h"

namespace WeeOS
{

namespace
{

constexpr std::string_view PcSettingsFileName = "WeeOS.settings.txt";

// The System Menu reads and writes the settings of the console as its own user.
constexpr Ios::Caller SystemMenuCaller = {Ios::SystemMenuUserId, Ios::SystemMenuGroupId};

Platform::ScreenShape ShapeOf(Settings::AspectRatio aspectRatio)
{
    if (aspectRatio == Settings::AspectRatio::FourByThree)
        return Platform::ScreenShape::FourByThree;

    return Platform::ScreenShape::SixteenByNine;
}

}

WiiSettingsServices::WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, Input::VirtualRemote& virtualRemote)
    : window(programWindow)
    , remote(virtualRemote)
    , systemSettings(iosServices.GetFileSystem(), SystemMenuCaller)
    , clock([this] { return systemSettings.GetClockBias(); })
    , formatter(iosServices.GetFileSystem(), iosServices.GetTitles(), iosServices.GetPower())
    , pcSettings(Platform::ExecutableFolder() / PcSettingsFileName)
    , flowServices{systemSettings, clock, iosServices.GetNetworkConfig(), SystemMenu::SearchWithRadio(iosServices.GetWireless()), formatter,
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
