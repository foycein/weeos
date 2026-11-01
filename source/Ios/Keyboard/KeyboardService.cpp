#include "Ios/Keyboard/KeyboardService.h"

#include "Ios/Keyboard/UsbKeyCodes.h"

#include <algorithm>

namespace Ios
{

namespace
{

// A report has places for six keys. For more keys than that, every place holds the roll over error.
PressedKeys MakeReportedKeys(std::span<const std::uint8_t> pressedKeys)
{
    PressedKeys reportedKeys = {};

    if (pressedKeys.size() > reportedKeys.size())
    {
        reportedKeys.fill(UsbKey::RollOverError);
        return reportedKeys;
    }

    std::copy(pressedKeys.begin(), pressedKeys.end(), reportedKeys.begin());
    return reportedKeys;
}

KeyboardEvent MakeEventWithoutKeys(KeyboardEventKind kind, std::uint32_t keyboardId)
{
    constexpr std::uint8_t noModifiers = 0;
    return KeyboardEvent{kind, keyboardId, noModifiers, PressedKeys{}};
}

}

KeyboardService::KeyboardService(const IosProfile& iosProfile)
    : profile(iosProfile)
{
}

void KeyboardService::ReportConnected(std::uint32_t keyboardId)
{
    if (profile.keyboardDriver == KeyboardDriver::None)
        return;

    std::lock_guard<std::mutex> lock(mutex);
    if (events.IsClosed())
        return;

    bool isAttachedAlready = keyboards.contains(keyboardId);
    if (isAttachedAlready)
        return;

    keyboards[keyboardId] = KeyboardState{};
    events.Post(MakeEventWithoutKeys(KeyboardEventKind::Connected, keyboardId));
}

void KeyboardService::ReportDisconnected(std::uint32_t keyboardId)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (events.IsClosed())
        return;

    bool wasAttached = keyboards.erase(keyboardId) > 0;
    if (!wasAttached)
        return;

    events.Post(MakeEventWithoutKeys(KeyboardEventKind::Disconnected, keyboardId));
}

void KeyboardService::ReportKeys(std::uint32_t keyboardId, std::uint8_t modifiers, std::span<const std::uint8_t> pressedKeys)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (events.IsClosed())
        return;

    auto found = keyboards.find(keyboardId);
    if (found == keyboards.end())
        return;

    KeyboardState& keyboard = found->second;
    PressedKeys reportedKeys = MakeReportedKeys(pressedKeys);

    bool isSameAsBefore = modifiers == keyboard.modifiers && reportedKeys == keyboard.pressedKeys;
    if (isSameAsBefore)
        return;

    keyboard.modifiers = modifiers;
    keyboard.pressedKeys = reportedKeys;
    events.Post(KeyboardEvent{KeyboardEventKind::KeysChanged, keyboardId, modifiers, reportedKeys});
}

std::optional<std::uint8_t> KeyboardService::GetLights(std::uint32_t keyboardId) const
{
    std::lock_guard<std::mutex> lock(mutex);

    auto found = keyboards.find(keyboardId);
    if (found == keyboards.end())
        return std::nullopt;

    return found->second.lightBits;
}

std::optional<KeyboardEvent> KeyboardService::WaitForEvent(std::stop_token stopToken)
{
    return events.Wait(stopToken);
}

std::optional<KeyboardEvent> KeyboardService::TryTakeEvent()
{
    return events.TryTake();
}

bool KeyboardService::SetLights(std::uint32_t keyboardId, std::uint8_t lightBits)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto found = keyboards.find(keyboardId);
    if (found == keyboards.end())
        return false;

    found->second.lightBits = lightBits;
    return true;
}

void KeyboardService::Close()
{
    events.Close();
}

}
