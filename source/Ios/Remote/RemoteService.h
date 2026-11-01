#pragma once

#include "Ios/EventQueue.h"

#include "Input/RemoteChannels.h"
#include "Input/VirtualRemote.h"

#include <array>
#include <cstddef>
#include <mutex>
#include <optional>

namespace Ios
{

// Remote 1 of the menu is channel 0. The virtual remote of the mouse and the keyboard sits there from the start.
constexpr int VirtualRemoteChannel = 0;

enum class RemoteEventKind
{
    // A device is on the channel and says it is connected.
    Connected,

    // The device is gone, or says it is not connected any more.
    Disconnected,

    // The battery level of a connected device is not the one of its previous state. The first state after a connect only sets the level.
    BatteryChanged,
};

struct RemoteEvent
{
    RemoteEventKind kind;
    int channel;

    bool operator==(const RemoteEvent&) const = default;
};

// The Wii Remotes, in place of the Bluetooth device /dev/usb/oh1/57e/305 and of the Bluetooth stack that the programs ran on top of it.
// The boundary is the device itself and not the radio: no inquiry, no link keys, no packets. A device that is attached to one of the four
// channels (Input::RemoteSource) tells what a remote tells: whether it is connected and with which extension, its buttons, its pointer,
// its battery, and it takes the outputs of the programs: rumble, the four lights and the speaker.
//
// The virtual remote of the Input part is attached to channel 0 from the start, so a remote is connected as soon as the service exists.
// The other channels stay empty until a device is attached. The program attaches the real Wii Remotes, Joy-Cons and gamepads the same way.
//
// The service is polled the way the menu polls WPAD and KPAD: in every frame it probes the channels and reads their new states.
// The devices cannot call the service. A connection that changes is noticed when the channel is probed or read next, and the event comes then.
//
// The service is thread safe. The devices are not: the service calls them one at a time, and the events of the window that drive
// the virtual remote must come from the thread that polls.
class RemoteService
{
public:
    RemoteService();

    // ---- The platform

    // The virtual remote 1. The program gives it to the window as its listener (PcKeyboard passes the events on to it) and tells it the size
    // of the window and the shape of the screen. The service starts it with the default window and a 16:9 screen.
    Input::VirtualRemote& GetVirtualRemote();

    // Puts a device on a channel. The device must stay alive until it is detached or the service is closed.
    // A device that sat on the channel is detached first. Channel numbers outside 0 to 3 throw an Input::InputError.
    void AttachSource(int channel, Input::RemoteSource& source);

    // Takes the device off the channel. A device that was connected is reported as disconnected. Nothing happens for an empty channel.
    void DetachSource(int channel);

    // ---- The programs, once in every frame

    // The sensor bar is one for the whole console. Every device is told, also a device that is attached later.
    void SetSensorBarPosition(Input::SensorBarPosition position);

    // The vibration setting of the console (BT.MOT). While it is off, a start of the rumble is dropped and a stop goes through.
    // It starts on.
    void SetRumbleEnabled(bool isEnabled);
    bool IsRumbleEnabled();

    // What is on the channel now: no device, a remote, or a remote with a Nunchuk or a Classic Controller.
    Input::DeviceKind Probe(int channel);

    // The state of the channel's device since the last read. Nothing for an empty channel or when the device has no new data.
    std::optional<Input::RemoteState> ReadNewState(int channel);

    // Ends the connection. The virtual remote connects again when one of its buttons is pressed.
    void Disconnect(int channel);

    // ---- The programs, when they want something from a remote. They do nothing for an empty channel.

    void SetRumble(int channel, bool shouldRumble);
    void SetPlayerLeds(int channel, const Input::PlayerLeds& leds);
    void PlaySpeakerSound(int channel, const Input::SpeakerSound& sound);

    // ---- The events

    // The connections and the batteries, in the order they were noticed. The first event is the connect of the virtual remote.
    EventQueue<RemoteEvent>& Events();

    // Ends the service: the devices are let go and never called again, whoever waits for an event stops waiting, and nothing new is
    // accepted. The events that are left can still be taken. The virtual remote stays where it is, so the window can still tell it things.
    void Close();

private:
    struct ChannelRecord
    {
        Input::DeviceKind deviceKind = Input::DeviceKind::None;
        std::optional<int> batteryLevel;
    };

    // The channel number must be valid. The calls to the channels have thrown for a wrong one before this is used.
    ChannelRecord& RecordOf(int channel);

    void ReleaseChannel(int channel);
    void NoticeConnection(int channel);
    void NoticeBattery(int channel, int batteryLevel);

    std::mutex mutex;
    bool isClosed = false;
    Input::VirtualRemote virtualRemote;
    Input::RemoteChannels channels;
    std::array<ChannelRecord, Input::RemoteChannelCount> records;
    EventQueue<RemoteEvent> events;
};

}
