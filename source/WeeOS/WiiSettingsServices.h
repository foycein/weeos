#pragma once

#include "Input/VirtualRemote.h"
#include "Ios/IosServices.h"
#include "Platform/RegionalSettings.h"
#include "Platform/ScreenShape.h"
#include "Platform/Window.h"
#include "Settings/PcSettings.h"
#include "Settings/SystemSettings.h"
#include "SystemMenu/WiiSettings/SettingsFlow.h"
#include "WeeOS/ProgramSound.h"

#include <filesystem>

namespace WeeOS
{

// What Wii Settings works with in WeeOS.exe: the settings of the console, the network, and the settings of the PC in the file
// WeeOS.settings.txt beside the program. It is also the part of the program that makes a change of the settings of the PC come true
// at once: the window, the keys and the mouse of the virtual Wii Remote, the sound device, the volume and the rumble.
class WiiSettingsServices : public Settings::PcSettingsTarget
{
public:
    // Reads the settings, sets the country of the console to the one of the PC and gives the window, the remotes and the sound the
    // settings of the PC. Everything given must outlive this object.
    WiiSettingsServices(Ios::IosServices& iosServices, Platform::Window& programWindow, ProgramSound& programSound,
        const Platform::RegionalSettings& regionalSettings, const std::filesystem::path& pcSettingsFile);

    SystemMenu::SettingsFlowServices& FlowServices();

    // The shape of the picture: the console's widescreen setting.
    Platform::ScreenShape PictureShape() const;

    // The sound mode of the console, as the settings hold it now (the Sound Mode page changes it before it is saved).
    Settings::SoundMode SoundMode() const;

    void ApplyPcSettings(const Settings::PcSettings& settings) override;
    void ApplyRumble(bool isRumbleOn) override;
    std::vector<std::u16string> ListAudioDevices() const override;

private:
    void TakeTheCountryOfThePc(const Platform::RegionalSettings& regionalSettings);

    Platform::Window& window;
    Ios::RemoteService& remotes;
    ProgramSound& sound;

    Settings::SystemSettings systemSettings;
    Settings::PcSettingsStore pcSettings;
    SystemMenu::SettingsFlowServices flowServices;
};

}
