#pragma once

#include "Settings/SystemSettings.h"

#include "Input/ButtonMap.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Settings
{

// The size of the window when nothing else is set (docs/DECISIONS.md: 1280 x 720). WeeOS starts in borderless fullscreen.
constexpr std::uint32_t DefaultWindowWidth = 1280;
constexpr std::uint32_t DefaultWindowHeight = 720;

constexpr int LowestVolume = 0;
constexpr int HighestVolume = 100;

// The settings of WeeOS on the PC, which a Wii does not have: the window, the sound device and the keys. They are the pages "PC Settings"
// of Wii Settings (docs/DECISIONS.md, Settings). The sound mode and the rumble are the console's own settings (SystemSettings).
struct PcSettings
{
    std::uint32_t windowWidth = DefaultWindowWidth;
    std::uint32_t windowHeight = DefaultWindowHeight;
    bool isFullscreen = true;

    // The name Windows gives the playback device. Empty for the device that Windows uses by default.
    std::u16string audioDevice;

    // How loud WeeOS plays, from 0 to 100 percent.
    int volume = HighestVolume;

    // One key for each button of the virtual Wii Remote that has one.
    std::vector<Input::KeyBinding> keyMap;

    // The left mouse button is A and the right one is B, or the other way round.
    bool areMouseButtonsSwapped = false;

    // The owner's own server, which takes the place of every Nintendo service (docs/DECISIONS.md, Online): a host name or an IPv4
    // address, with ":port" when it is not 80. Empty until the server exists; until then every online channel shows its server error.
    std::string serverAddress;

    bool operator==(const PcSettings& other) const;
};

// The settings that WeeOS starts with: the window of DECISIONS.md, the default sound device, and the keys of Input::DefaultKeyMap.
PcSettings DefaultPcSettings();

// The key that presses the button, or Settings::NoKey.
int KeyOfButton(const PcSettings& settings, Input::ButtonMask button);
void SetKeyOfButton(PcSettings& settings, Input::ButtonMask button, int virtualKeyCode);

// The mouse buttons as the virtual Wii Remote takes them.
std::vector<Input::MouseBinding> MouseMapOf(const PcSettings& settings);

// The settings as a plain text file: one setting per line, "Name = value", lines that start with # are notes.
// A line that WeeOS does not know, or whose value it cannot read, leaves that setting as it is by default.
std::string WritePcSettingsText(const PcSettings& settings);
PcSettings ReadPcSettingsText(std::string_view text);

// The file beside the program that holds the settings, and the settings that are in force now.
class PcSettingsStore
{
public:
    // Reads the file. A file that is not there gives the default settings.
    explicit PcSettingsStore(std::filesystem::path settingsFile);

    const PcSettings& Values() const;

    // Takes the new settings and writes the file. Throws a SettingsError when the file cannot be written.
    void Store(const PcSettings& newSettings);

private:
    std::filesystem::path file;
    PcSettings values;
};

// The program that makes a change of the settings come true at once: it resizes the window, gives the keys to the remote, sets the
// volume, lets the remotes rumble or not. The pages call it as soon as the user changes something.
class PcSettingsTarget
{
public:
    virtual ~PcSettingsTarget() = default;

    virtual void ApplyPcSettings(const PcSettings& settings) = 0;

    // The console's rumble setting changed.
    virtual void ApplyRumble(bool isRumbleOn) = 0;

    // The names of the playback devices of the PC, as the audio page lists them.
    virtual std::vector<std::u16string> ListAudioDevices() const = 0;
};

}
