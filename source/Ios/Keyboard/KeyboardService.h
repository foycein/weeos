#pragma once

#include "Ios/EventQueue.h"
#include "Ios/Profile/ProfileChoice.h"

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>

namespace Ios
{

// A report of a USB keyboard has places for six keys.
constexpr int KeysPerReport = 6;
using PressedKeys = std::array<std::uint8_t, KeysPerReport>;

enum class KeyboardEventKind
{
    Connected,
    Disconnected,

    // Keys went down or up. The event holds all the keys that are down now.
    KeysChanged,
};

// The number that the original keyboard device writes first in an event. The native code does not need it.
// It is here so that a log line, or a result measured on the console, can be compared with ours.
constexpr std::uint32_t ConnectedEventType = 0;
constexpr std::uint32_t DisconnectedEventType = 1;
constexpr std::uint32_t KeysEventType = 2;

constexpr std::uint32_t OriginalEventType(KeyboardEventKind kind)
{
    if (kind == KeyboardEventKind::Connected)
        return ConnectedEventType;

    if (kind == KeyboardEventKind::Disconnected)
        return DisconnectedEventType;

    return KeysEventType;
}

// What the original device hands to a program: the kind, the id of the keyboard, the modifier byte and the six places for keys.
// A place that holds no key is 0. The modifiers and the keys of a Connected or Disconnected event are empty.
struct KeyboardEvent
{
    KeyboardEventKind kind;
    std::uint32_t keyboardId;
    std::uint8_t modifiers;
    PressedKeys pressedKeys;

    bool operator==(const KeyboardEvent&) const = default;
};

// The USB keyboards, in place of /dev/usb/kbd and of the HID device that the Wii Shop and the Internet Channel use.
// The codes of the keys are the usage ids of USB (UsbKeyCodes.h). The layout, which letter a code makes, is the job of the program.
//
// The platform attaches a keyboard and reports its keys, as a keyboard reports them over USB. The programs wait for the events.
// Thread safe. An IOS that has no keyboard driver (the profile says KeyboardDriver::None) sees no keyboard: it never gets an event.
//
// Like the module of the original, the service
//   * tells every change in one event that holds all the keys that are down, and tells nothing when a report equals the one before,
//   * has six places for keys and fills all of them with UsbKey::RollOverError when more keys are down,
//   * switches the lights of a keyboard off when it is attached.
// It does not repeat keys. The original does not either: the repeat belongs to the keyboard library of the programs (docs/audit/Ios.md).
//
// Events are kept until a program takes them (deviation D1 in docs/audit/Ios.md).
class KeyboardService
{
public:
    // Without a profile this is the keyboard driver of IOS80.
    explicit KeyboardService(const IosProfile& iosProfile = SystemMenuProfile());

    // ---- The platform

    // A keyboard is there. Nothing happens when the keyboard is attached already. A new keyboard has no key down and no light on.
    void ReportConnected(std::uint32_t keyboardId);

    // The keyboard is gone. Nothing happens when there is no such keyboard.
    void ReportDisconnected(std::uint32_t keyboardId);

    // All the keys that are down now, in the order the keyboard reports them, and the modifiers.
    // More keys than a report holds give the roll over error. Nothing happens when the keyboard is not attached.
    void ReportKeys(std::uint32_t keyboardId, std::uint8_t modifiers, std::span<const std::uint8_t> pressedKeys);

    // The lights that the programs asked for, as the bits of KeyboardLight. Nothing when the keyboard is not attached.
    std::optional<std::uint8_t> GetLights(std::uint32_t keyboardId) const;

    // ---- The programs

    // Waits until there is an event and gives it back. The events come in the order the platform reported them.
    // A keyboard that was attached before the program started to wait has its Connected event waiting, so it comes first.
    // Gives back nothing when the stop token asks to stop, or when the service was closed and no event is left.
    std::optional<KeyboardEvent> WaitForEvent(std::stop_token stopToken);

    // Does not wait. Gives back nothing when there is no event.
    std::optional<KeyboardEvent> TryTakeEvent();

    // Sets the lights of one keyboard, the bits of KeyboardLight. Gives back false when the keyboard is not attached.
    // The write of the original with the high bit set, which switches the lights of all keyboards off, is not built: no program writes it.
    bool SetLights(std::uint32_t keyboardId, std::uint8_t lightBits);

    // ---- The end

    // Ends the service: whoever waits stops waiting, and nothing new is accepted. The events that are left can still be taken.
    void Close();

private:
    struct KeyboardState
    {
        std::uint8_t modifiers = 0;
        PressedKeys pressedKeys = {};
        std::uint8_t lightBits = 0;
    };

    const IosProfile profile;
    mutable std::mutex mutex;
    std::map<std::uint32_t, KeyboardState> keyboards;
    EventQueue<KeyboardEvent> events;
};

}
