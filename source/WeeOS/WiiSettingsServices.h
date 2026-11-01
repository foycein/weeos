#pragma once

#include "Input/VirtualRemote.h"
#include "Ios/IosServices.h"
#include "Platform/Window.h"
#include "Settings/ConsoleClock.h"
#include "Settings/PcSettings.h"
#include "Settings/SystemMemoryFormatter.h"
#include "Settings/SystemSettings.h"
#include "SystemMenu/WiiSettings/SettingsFlow.h"
#include "WeeOS/ProgramSound.h"


namespace WeeOS
{

// What Wii Settings works with in WeeOS.exe: the settings of the console, its clock, the network and the format, and the settings of
// the PC in the file WeeOS.settings.txt beside the program. It is also the part of the program that makes a change of the settings of
// the PC come true at once: the window, the keys and the mouse of the virtual Wii Remote, the sound device and the volume.
// The picture is always 16:9: the console's widescreen setting is kept at 16:9 (docs/DECISIONS.md, Settings).
class WiiSettingsServices : public Settings::PcSettingsTarget
{
public:
    // Reads the settings of the console, keeps its picture 16:9 and gives the remote and the sound the settings of the PC. The window
    // keeps the place the program gave it. Everything given must outlive this object.
    WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, Input::VirtualRemote& virtualRemote,
        ProgramSound& programSound, Settings::PcClock pcClock, Settings::PcSettingsStore& pcSettingsStore);

    SystemMenu::SettingsFlowServices& FlowServices();

    // The sound mode of the console, as the settings hold it now (the Sound page changes it before it is saved).
    Settings::SoundMode SoundMode() const;

    void ApplyPcSettings(const Settings::PcSettings& settings) override;

    // The picture stays 16:9, so a page that changes the widescreen setting only sees it go back to 16:9.
    void ApplyPictureShape(Settings::AspectRatio aspectRatio) override;

    std::vector<std::u16string> ListAudioDevices() const override;

private:
    void ApplyControlsAndSound(const Settings::PcSettings& settings);
    void KeepThePictureWide();

    Platform::Window& window;
    Input::VirtualRemote& remote;
    ProgramSound& sound;

    Settings::SystemSettings systemSettings;
    Settings::ConsoleClock clock;
    Settings::SystemMemoryFormatter formatter;
    SystemMenu::SettingsFlowServices flowServices;
};

}
