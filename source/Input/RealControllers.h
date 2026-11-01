#pragma once

#include "Input/ControllerSettings.h"
#include "Input/GamepadRemote.h"
#include "Input/JoyCon.h"
#include "Input/JoyConRemote.h"
#include "Input/WiiRemote.h"

#include "Platform/InputDevices.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Input
{

// Nintendo's number for its devices, and the products WeeOS knows (the Wii Remote Plus has its own number).
constexpr std::uint16_t NintendoVendorId = 0x057E;
constexpr std::uint16_t WiiRemoteProductId = 0x0306;
constexpr std::uint16_t WiiRemotePlusProductId = 0x0330;
constexpr std::uint16_t LeftJoyConProductId = 0x2006;
constexpr std::uint16_t RightJoyConProductId = 0x2007;

// The real controllers of the PC as remotes of the console: Wii Remotes (over Bluetooth or through a Dolphin Bar in its Wii Remote
// mode), Joy-Cons (a right one and a left one together are a Wii Remote with a Nunchuk) and gamepads of the Xbox kind.
//
// Every two seconds it looks for devices that came; a device that went is let go. A controller that is connected gets the lowest
// free channel and keeps it while it stays connected; one that is not connected any more (switched off, or disconnected by the HOME
// menu) gives its channel back. A fifth controller waits for a free channel.
class RealControllers
{
public:
    // The access must outlive this object.
    explicit RealControllers(Platform::InputDeviceAccess& deviceAccess);

    // Once in every step, with the seconds of a clock that never goes backward. The sources of the channels may change; a source
    // that is gone is destroyed here, so the program gives the remote service the new sources right after every Update.
    void Update(double nowInSeconds);

    // The real controller on the channel (0 to 3), or null.
    RemoteSource* SourceOfChannel(int channel) const;

    // The setting BT.SENS of the console, for the cameras of the Wii Remotes.
    void SetCameraSensitivity(int level);

    void SetControllerSettings(const ControllerSettings& newSettings);

    // A line for every device and what it is doing, for the hardware check program.
    std::vector<std::string> DescribeDevices() const;

private:
    struct OpenWiiRemote
    {
        std::wstring path;
        std::unique_ptr<WiiRemote> remote;
    };

    struct OpenJoyCon
    {
        std::wstring path;
        std::unique_ptr<JoyCon> joyCon;
    };

    void FindNewDevices();
    bool IsOpen(const std::wstring& path) const;
    void PollDevices();
    void ForgetGoneDevices();
    void PairJoyCons(double elapsedSeconds);
    void ReadGamepads(double elapsedSeconds, bool isSearchDue);
    void AssignChannels();
    void ReleaseChannelOf(const RemoteSource* source);
    std::vector<RemoteSource*> AllRemotes();
    std::string ChannelText(const RemoteSource* source) const;

    Platform::InputDeviceAccess& devices;
    double now = 0;
    double lastSearchTime = 0;
    bool hasSearched = false;

    std::vector<OpenWiiRemote> wiiRemotes;
    std::vector<OpenJoyCon> joyCons;
    std::vector<std::unique_ptr<JoyConRemote>> joyConRemotes;
    std::array<GamepadRemote, Platform::GamepadSlotCount> gamepads;
    std::array<bool, Platform::GamepadSlotCount> isGamepadInSlot = {};
    std::array<bool, Platform::GamepadSlotCount> isGamepadRumbling = {};

    std::array<RemoteSource*, RemoteChannelCount> channels = {};
    ControllerSettings settings;
    int cameraSensitivity = UsualCameraSensitivity;
};

}
