// ControllerCheck: shows what WeeOS sees of the controllers of this PC, without starting WeeOS.
//
//   ControllerCheck [--seconds N] [--bar-above]
//
// It lists the devices of Windows that WeeOS looks at (Nintendo's devices and the gamepad slots), then shows the four remotes
// twice in a second: what is connected, the buttons, the hand, the roll, the battery, the Nunchuk and the Classic Controller.
// A held A lets the controller rumble, + plays a short tone on the speaker of a Wii Remote. It ends after N seconds (60 by default).

#include "Input/RealControllers.h"
#include "Input/RemoteChannels.h"

#include "Platform/InputDevices.h"
#include "Platform/PreciseTimer.h"

#include <windows.h>

#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr double SecondsOfAStep = 1001.0 / 60000.0;
constexpr double SecondsBetweenPrints = 0.5;
constexpr double UsualRunSeconds = 60.0;

// A tone of 440 Hz for half a second, at the rate of the remote's speaker.
constexpr double ToneFrequency = 440.0;
constexpr double ToneSeconds = 0.5;
constexpr double ToneLoudness = 8000.0;

struct ButtonName
{
    Input::ButtonMask button;
    const char* name;
};

constexpr ButtonName RemoteButtonNames[] = {
    {Input::RemoteButtons::A, "A"},
    {Input::RemoteButtons::B, "B"},
    {Input::RemoteButtons::One, "1"},
    {Input::RemoteButtons::Two, "2"},
    {Input::RemoteButtons::Plus, "+"},
    {Input::RemoteButtons::Minus, "-"},
    {Input::RemoteButtons::Home, "HOME"},
    {Input::RemoteButtons::Up, "Up"},
    {Input::RemoteButtons::Down, "Down"},
    {Input::RemoteButtons::Left, "Left"},
    {Input::RemoteButtons::Right, "Right"},
    {Input::RemoteButtons::NunchukC, "C"},
    {Input::RemoteButtons::NunchukZ, "Z"},
};

constexpr ButtonName ClassicButtonNames[] = {
    {Input::ClassicButtons::A, "a"},
    {Input::ClassicButtons::B, "b"},
    {Input::ClassicButtons::X, "x"},
    {Input::ClassicButtons::Y, "y"},
    {Input::ClassicButtons::LeftTrigger, "L"},
    {Input::ClassicButtons::RightTrigger, "R"},
    {Input::ClassicButtons::LeftZ, "ZL"},
    {Input::ClassicButtons::RightZ, "ZR"},
    {Input::ClassicButtons::Plus, "+"},
    {Input::ClassicButtons::Minus, "-"},
    {Input::ClassicButtons::Home, "HOME"},
    {Input::ClassicButtons::Up, "Up"},
    {Input::ClassicButtons::Down, "Down"},
    {Input::ClassicButtons::Left, "Left"},
    {Input::ClassicButtons::Right, "Right"},
};

struct Options
{
    double seconds = UsualRunSeconds;
    Input::SensorBarPosition sensorBarPosition = Input::SensorBarPosition::BelowScreen;
};

Options ReadOptions(int argumentCount, char** arguments)
{
    Options options;
    for (int index = 1; index < argumentCount; ++index)
    {
        std::string argument = arguments[index];
        if (argument == "--seconds" && index + 1 < argumentCount)
            options.seconds = std::stod(arguments[++index]);
        else if (argument == "--bar-above")
            options.sensorBarPosition = Input::SensorBarPosition::AboveScreen;
    }
    return options;
}

template <std::size_t Count>
std::string NamesOf(Input::ButtonMask buttons, const ButtonName (&names)[Count])
{
    std::string text;
    for (const ButtonName& name : names)
    {
        if ((buttons & name.button) == 0)
            continue;

        if (!text.empty())
            text += " ";
        text += name.name;
    }
    return text.empty() ? "-" : text;
}

std::string NarrowText(const std::wstring& text)
{
    std::string narrow;
    for (wchar_t character : text)
        narrow.push_back(character < 0x80 ? static_cast<char>(character) : '?');
    return narrow;
}

std::string KindName(Input::DeviceKind kind)
{
    switch (kind)
    {
    case Input::DeviceKind::Remote:
        return "Wii Remote";
    case Input::DeviceKind::RemoteWithNunchuk:
        return "Wii Remote with Nunchuk";
    case Input::DeviceKind::RemoteWithClassicController:
        return "Wii Remote with Classic Controller";
    default:
        return "nothing";
    }
}

void ListDevicesOfWindows(Platform::InputDeviceAccess& devices)
{
    std::cout << "Devices of Windows that WeeOS looks at:\n";
    int found = 0;
    for (const Platform::HidDeviceInfo& device : devices.ListHidDevices())
    {
        if (device.vendorId != Input::NintendoVendorId)
            continue;

        std::cout << std::format("  {:04X}:{:04X} {}\n", device.vendorId, device.productId, NarrowText(device.productName));
        ++found;
    }

    for (int slot = 0; slot < Platform::GamepadSlotCount; ++slot)
    {
        if (!devices.ReadGamepad(slot))
            continue;

        std::cout << std::format("  Xbox gamepad in slot {}\n", slot + 1);
        ++found;
    }

    if (found == 0)
        std::cout << "  none. Pair a Wii Remote in the Bluetooth settings of Windows (press 1 and 2), put a Dolphin Bar in mode 4,\n"
                     "  pair the Joy-Cons, or plug in a gamepad.\n";
}

Input::SpeakerSound MakeTone()
{
    Input::SpeakerSound tone;
    int sampleCount = static_cast<int>(Input::RemoteSpeakerSampleRate * ToneSeconds);
    for (int index = 0; index < sampleCount; ++index)
    {
        double phase = 2.0 * std::numbers::pi * ToneFrequency * index / Input::RemoteSpeakerSampleRate;
        tone.samples.push_back(static_cast<std::int16_t>(ToneLoudness * std::sin(phase)));
    }
    return tone;
}

void PrintRemote(int channel, Input::DeviceKind kind, const Input::RemoteState& state)
{
    std::cout << std::format("Remote {}: {}, battery {}/4\n", channel + 1, KindName(kind), state.batteryLevel);
    std::cout << std::format("  buttons: {}\n", NamesOf(state.buttonsHeld, RemoteButtonNames));

    if (state.pointerIsValid)
        std::cout << std::format("  hand: x {:+.3f} y {:+.3f}, roll {:+.1f} degrees, {:.2f} m from the bar\n", state.pointer.x,
            state.pointer.y, state.rollDegrees, state.distanceInMeters);
    else
        std::cout << std::format("  hand: not on the screen, roll {:+.1f} degrees\n", state.rollDegrees);

    if (kind == Input::DeviceKind::RemoteWithNunchuk)
        std::cout << std::format("  Nunchuk stick: {:+.2f} {:+.2f}\n", state.nunchuk.stick.x, state.nunchuk.stick.y);

    if (kind == Input::DeviceKind::RemoteWithClassicController)
    {
        const Input::ClassicControllerState& classic = state.classic;
        std::cout << std::format("  Classic: {}; sticks {:+.2f} {:+.2f} / {:+.2f} {:+.2f}; triggers {:.2f} {:.2f}\n",
            NamesOf(classic.buttonsHeld, ClassicButtonNames), classic.leftStick.x, classic.leftStick.y, classic.rightStick.x,
            classic.rightStick.y, classic.leftTrigger, classic.rightTrigger);
    }
}

}

int main(int argumentCount, char** arguments)
{
    Options options = ReadOptions(argumentCount, arguments);

    Platform::WindowsInputDevices devices;
    ListDevicesOfWindows(devices);
    std::cout << "\nA held A rumbles, + plays a tone on a Wii Remote. The program ends after " << options.seconds << " seconds.\n\n";

    Input::RealControllers controllers(devices);
    Input::RemoteChannels channels;
    channels.SetSensorBarPosition(options.sensorBarPosition);
    std::array<Input::RemoteSource*, Input::RemoteChannelCount> attached = {};
    std::array<Input::RemoteState, Input::RemoteChannelCount> latestStates = {};

    Input::SpeakerSound tone = MakeTone();
    Platform::PreciseTimer timer;
    double controllerSeconds = 0;
    double lastPrint = 0;

    while (timer.ElapsedSeconds() < options.seconds)
    {
        controllerSeconds += SecondsOfAStep;
        controllers.Update(controllerSeconds);

        for (int channel = 0; channel < Input::RemoteChannelCount; ++channel)
        {
            Input::RemoteSource* source = controllers.SourceOfChannel(channel);
            if (source != attached[channel])
            {
                if (source != nullptr)
                    channels.Attach(channel, *source);
                else
                    channels.Detach(channel);
                attached[channel] = source;
            }

            if (channels.Probe(channel) == Input::DeviceKind::None)
                continue;

            std::optional<Input::RemoteState> state = channels.Read(channel);
            if (!state)
                continue;

            latestStates[channel] = *state;
            channels.SetRumble(channel, (state->buttonsHeld & Input::RemoteButtons::A) != 0);
            if ((state->buttonsPressed & Input::RemoteButtons::Plus) != 0)
                channels.PlaySpeakerSound(channel, tone);
        }

        if (timer.ElapsedSeconds() - lastPrint >= SecondsBetweenPrints)
        {
            lastPrint = timer.ElapsedSeconds();
            std::cout << std::format("---- {:.1f} s\n", lastPrint);
            for (const std::string& line : controllers.DescribeDevices())
                std::cout << line << "\n";

            for (int channel = 0; channel < Input::RemoteChannelCount; ++channel)
            {
                Input::DeviceKind kind = channels.Probe(channel);
                if (kind != Input::DeviceKind::None)
                    PrintRemote(channel, kind, latestStates[channel]);
            }
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(SecondsOfAStep));
    }

    return 0;
}
