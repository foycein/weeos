#include "Input/SharedRemote.h"

#include <cmath>

namespace Input
{

namespace
{

// A real pointer that moves less than this has not moved: the hand of a remote held still trembles a little.
constexpr float SmallestRealMove = 0.002f;

bool HasMoved(const std::optional<Formats::Vector2>& before, Formats::Vector2 now, float smallestMove)
{
    if (!before)
        return true;

    float distance = std::hypot(now.x - before->x, now.y - before->y);
    return distance > smallestMove;
}

}

SharedRemote::SharedRemote(VirtualRemote& virtualRemote)
    : mouse(virtualRemote)
{
}

void SharedRemote::SetRealSource(RemoteSource* source)
{
    if (source == real)
        return;

    real = source;
    lastRealState.reset();
    lastRealPointer.reset();
    if (real != nullptr)
        real->SetPlayerLeds(playerLeds);
}

DeviceKind SharedRemote::Probe() const
{
    if (IsRealConnected())
        return real->Probe();

    return mouse.Probe();
}

std::optional<RemoteState> SharedRemote::ReadNewState()
{
    std::optional<RemoteState> mouseState = mouse.ReadNewState();
    std::optional<RemoteState> realState;
    if (IsRealConnected())
        realState = real->ReadNewState();
    else
        lastRealState.reset();

    if (!mouseState && !realState)
        return std::nullopt;

    NoticeWhichPointerMoved(mouseState, realState);
    if (realState)
        lastRealState = realState;

    RemoteState state = mouseState.value_or(RemoteState());
    ButtonMask held = mouseState ? mouseState->buttonsHeld : 0;

    if (lastRealState)
    {
        held |= lastRealState->buttonsHeld;
        state.batteryLevel = lastRealState->batteryLevel;
        state.nunchuk = lastRealState->nunchuk;
        state.classic = lastRealState->classic;

        bool realPointsAtTheScreen = lastRealState->pointerIsValid;
        bool mouseIsOverTheWindow = mouseState && mouseState->pointerIsValid;
        bool usesRealPointer = realPointsAtTheScreen && (!mouseMovedLast || !mouseIsOverTheWindow);
        if (usesRealPointer)
        {
            state.pointerIsValid = true;
            state.pointer = lastRealState->pointer;
            state.rollDegrees = lastRealState->rollDegrees;
            state.distanceInMeters = lastRealState->distanceInMeters;
        }
    }

    // The edges of the Classic Controller belong to the read they came with.
    if (!realState && lastRealState)
    {
        state.classic.buttonsPressed = 0;
        state.classic.buttonsReleased = 0;
    }

    state.buttonsHeld = held;
    state.buttonsPressed = held & ~buttonsHeldAtLastRead;
    state.buttonsReleased = buttonsHeldAtLastRead & ~held;
    buttonsHeldAtLastRead = held;
    return state;
}

void SharedRemote::Disconnect()
{
    mouse.Disconnect();
    if (real != nullptr)
        real->Disconnect();
}

void SharedRemote::SetSensorBarPosition(SensorBarPosition position)
{
    mouse.SetSensorBarPosition(position);
    if (real != nullptr)
        real->SetSensorBarPosition(position);
}

void SharedRemote::SetRumble(bool shouldRumble)
{
    mouse.SetRumble(shouldRumble);
    if (real != nullptr)
        real->SetRumble(shouldRumble);
}

void SharedRemote::SetPlayerLeds(const PlayerLeds& leds)
{
    playerLeds = leds;
    mouse.SetPlayerLeds(leds);
    if (real != nullptr)
        real->SetPlayerLeds(leds);
}

void SharedRemote::PlaySpeakerSound(const SpeakerSound& sound)
{
    if (IsRealConnected())
        real->PlaySpeakerSound(sound);
    else
        mouse.PlaySpeakerSound(sound);
}

bool SharedRemote::IsRealConnected() const
{
    return real != nullptr && real->Probe() != DeviceKind::None;
}

void SharedRemote::NoticeWhichPointerMoved(const std::optional<RemoteState>& mouseState, const std::optional<RemoteState>& realState)
{
    if (mouseState && mouseState->pointerIsValid)
    {
        if (lastMousePointer && HasMoved(lastMousePointer, mouseState->pointer, 0))
            mouseMovedLast = true;
        lastMousePointer = mouseState->pointer;
    }

    if (realState && realState->pointerIsValid)
    {
        if (HasMoved(lastRealPointer, realState->pointer, SmallestRealMove))
        {
            mouseMovedLast = false;
            lastRealPointer = realState->pointer;
        }
    }
}

}
