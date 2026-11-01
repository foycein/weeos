#pragma once

#include "Input/RemoteSource.h"
#include "Input/VirtualRemote.h"

#include <optional>

namespace Input
{

// Remote 1 while a real controller is the first one: the controller and the mouse and keyboard together, so the PC's own controls
// never stop working. The buttons of both count. The hand is the pointer source "Auto" of the owner's decisions
// (OpenPoints/Input.md, INP-12): the real controller's while it points at the screen, unless the mouse moved after it did.
class SharedRemote final : public RemoteSource
{
public:
    // The virtual remote must outlive this object.
    explicit SharedRemote(VirtualRemote& virtualRemote);

    // The real controller on remote 1, or null for the mouse and keyboard alone. It must stay alive while it is set.
    void SetRealSource(RemoteSource* source);

    DeviceKind Probe() const override;
    std::optional<RemoteState> ReadNewState() override;
    void Disconnect() override;
    void SetSensorBarPosition(SensorBarPosition position) override;
    void SetRumble(bool shouldRumble) override;
    void SetPlayerLeds(const PlayerLeds& leds) override;
    void PlaySpeakerSound(const SpeakerSound& sound) override;

private:
    bool IsRealConnected() const;
    void NoticeWhichPointerMoved(const std::optional<RemoteState>& mouseState, const std::optional<RemoteState>& realState);

    VirtualRemote& mouse;
    RemoteSource* real = nullptr;

    std::optional<RemoteState> lastRealState;
    std::optional<Formats::Vector2> lastMousePointer;
    std::optional<Formats::Vector2> lastRealPointer;
    bool mouseMovedLast = false;
    ButtonMask buttonsHeldAtLastRead = 0;
};

}
