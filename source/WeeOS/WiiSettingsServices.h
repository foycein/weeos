#pragma once

#include "Input/VirtualRemote.h"
#include "Ios/IosServices.h"
#include "Platform/ScreenShape.h"
#include "Platform/Window.h"
#include "Settings/ConsoleClock.h"
#include "Settings/PcSettings.h"
#include "Settings/SystemMemoryFormatter.h"
#include "Settings/SystemSettings.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/WiiSettings/SettingsFlow.h"
#include "WeeOS/ProgramSound.h"

#include <filesystem>
#include <optional>

namespace WeeOS
{

// What Wii Settings works with in WeeOS.exe: the settings of the console, its clock, the network and the format, and the settings of
// the PC in the file WeeOS.settings.txt beside the program. It is also the part of the program that makes a change of the settings of
// the PC come true at once: the window, the keys and the mouse of the virtual Wii Remote, the shape of the picture, the sound device
// and the volume.
class WiiSettingsServices : public Settings::PcSettingsTarget
{
public:
    // Reads the settings and gives the window and the remote the settings of the PC. Everything given must outlive this object.
    WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, Input::VirtualRemote& virtualRemote,
        ProgramSound& programSound, Settings::PcClock pcClock, const std::filesystem::path& pcSettingsFile);

    SystemMenu::SettingsFlowServices& FlowServices();

    // The shape of the picture: the console's widescreen setting.
    Platform::ScreenShape PictureShape() const;

    // The sound mode of the console, as the settings hold it now (the Sound page changes it before it is saved).
    Settings::SoundMode SoundMode() const;

    // The menu shows its pictures on this display from now on, so a new shape reaches it.
    void UseDisplay(SystemMenu::Display& menuDisplay);

    void ApplyPcSettings(const Settings::PcSettings& settings) override;
    void ApplyPictureShape(Settings::AspectRatio aspectRatio) override;
    std::vector<std::u16string> ListAudioDevices() const override;

private:
    Platform::Window& window;
    Input::VirtualRemote& remote;
    ProgramSound& sound;
    SystemMenu::Display* display = nullptr;

    Settings::SystemSettings systemSettings;
    Settings::ConsoleClock clock;
    Settings::SystemMemoryFormatter formatter;
    Settings::PcSettingsStore pcSettings;
    SystemMenu::SettingsFlowServices flowServices;
};

}
