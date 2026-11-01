#pragma once

#include "Platform/GamepadReading.h"
#include "Platform/HidConnection.h"

#include <memory>
#include <optional>
#include <vector>

namespace Platform
{

// The controllers of the PC: the devices of the class "human interface device" and the gamepads of the Xbox kind.
// WindowsInputDevices asks Windows; a test gives devices of its own.
class InputDeviceAccess
{
public:
    InputDeviceAccess() = default;
    InputDeviceAccess(const InputDeviceAccess&) = delete;
    InputDeviceAccess& operator=(const InputDeviceAccess&) = delete;
    virtual ~InputDeviceAccess() = default;

    // Every such device that is there now.
    virtual std::vector<HidDeviceInfo> ListHidDevices() = 0;

    // Opens the device to send and take reports. Nothing when it cannot be opened, for example when it is gone.
    virtual std::unique_ptr<HidConnection> OpenHidDevice(const HidDeviceInfo& device) = 0;

    // The gamepad in the slot (0 to 3) now. Nothing while no gamepad is in the slot.
    virtual std::optional<GamepadReading> ReadGamepad(int slot) = 0;

    virtual void SetGamepadRumble(int slot, bool shouldRumble) = 0;
};

// The controllers as Windows sees them: the devices through the HID functions of Windows, the gamepads through XInput.
class WindowsInputDevices final : public InputDeviceAccess
{
public:
    std::vector<HidDeviceInfo> ListHidDevices() override;
    std::unique_ptr<HidConnection> OpenHidDevice(const HidDeviceInfo& device) override;
    std::optional<GamepadReading> ReadGamepad(int slot) override;
    void SetGamepadRumble(int slot, bool shouldRumble) override;
};

}
