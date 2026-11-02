#include "Input/JoyConRemote.h"

#include <algorithm>

namespace Input
{

namespace
{

// A stick pushed this far counts as a press of the cross in its direction.
constexpr float StickAsCrossFrom = 0.5f;

// Assumed: a Joy-Con held like a Wii Remote, its top pointing at the screen and its
// buttons up, turns right around its z axis (the one out of its face) the negative way, and up around its y axis (across it); the
// left Joy-Con's y axis points the other way.
struct TurnAxes
{
    float rightwardSignOfZ;
    float upwardSignOfY;
};

constexpr TurnAxes RightJoyConAxes = {-1.0f, 1.0f};
constexpr TurnAxes LeftJoyConAxes = {-1.0f, -1.0f};

template <std::size_t BindingCount>
ButtonMask MapButtons(std::uint32_t joyConButtons, const std::array<JoyConBinding, BindingCount>& map)
{
    ButtonMask buttons = 0;
    for (const JoyConBinding& binding : map)
    {
        if ((joyConButtons & binding.joyConButton) != 0)
            buttons |= binding.button;
    }
    return buttons;
}

ButtonMask CrossOfStick(Formats::Vector2 stick)
{
    ButtonMask buttons = 0;
    if (stick.x >= StickAsCrossFrom)
        buttons |= RemoteButtons::Right;
    if (stick.x <= -StickAsCrossFrom)
        buttons |= RemoteButtons::Left;
    if (stick.y >= StickAsCrossFrom)
        buttons |= RemoteButtons::Up;
    if (stick.y <= -StickAsCrossFrom)
        buttons |= RemoteButtons::Down;
    return buttons;
}

}

JoyConRemote::JoyConRemote(JoyCon* rightJoyCon, JoyCon* leftJoyCon)
    : right(rightJoyCon)
    , left(leftJoyCon)
{
}

JoyCon* JoyConRemote::Right() const
{
    return right;
}

JoyCon* JoyConRemote::Left() const
{
    return left;
}

void JoyConRemote::Update(double elapsedSeconds)
{
    bool rightHasNewData = right != nullptr && right->TakeNewData();
    bool leftHasNewData = left != nullptr && left->TakeNewData();
    if (rightHasNewData || leftHasNewData)
        hasNewData = true;

    ButtonMask buttons = HeldButtons();
    ButtonMask newlyPressed = buttons & ~buttonsOfLastUpdate;
    buttonsOfLastUpdate = buttons;
    if (newlyPressed != 0)
        isAsleep = false;

    MoveHand(elapsedSeconds);
}

void JoyConRemote::SetPointer(JoyConPointer pointer)
{
    pointerKind = pointer;
}

void JoyConRemote::SetPointerSpeed(int speed)
{
    hand.SetSpeed(speed);
}

DeviceKind JoyConRemote::Probe() const
{
    if (isAsleep || !PointingJoyCon().IsConnected())
        return DeviceKind::None;

    bool hasNunchuk = right != nullptr && left != nullptr && left->IsConnected();
    return hasNunchuk ? DeviceKind::RemoteWithNunchuk : DeviceKind::Remote;
}

std::optional<RemoteState> JoyConRemote::ReadNewState()
{
    DeviceKind kind = Probe();
    if (kind == DeviceKind::None || !hasNewData)
        return std::nullopt;

    hasNewData = false;

    RemoteState state;
    state.buttonsHeld = HeldButtons();
    state.buttonsPressed = state.buttonsHeld & ~buttonsHeldAtLastRead;
    state.buttonsReleased = buttonsHeldAtLastRead & ~state.buttonsHeld;
    buttonsHeldAtLastRead = state.buttonsHeld;

    state.pointerIsValid = true;
    state.pointer = hand.Pointer();
    state.batteryLevel = PointingJoyCon().BatteryLevel();

    if (kind == DeviceKind::RemoteWithNunchuk)
    {
        state.nunchuk.stick = left->Stick();
        state.batteryLevel = std::min(state.batteryLevel, left->BatteryLevel());
    }
    return state;
}

void JoyConRemote::Disconnect()
{
    isAsleep = true;
    buttonsHeldAtLastRead = 0;
    SetRumble(false);
}

void JoyConRemote::SetSensorBarPosition(SensorBarPosition)
{
}

void JoyConRemote::SetRumble(bool shouldRumble)
{
    PointingJoyCon().SetRumble(shouldRumble);
}

void JoyConRemote::SetPlayerLeds(const PlayerLeds& leds)
{
    if (right != nullptr)
        right->SetPlayerLeds(leds);
    if (left != nullptr)
        left->SetPlayerLeds(leds);
}

void JoyConRemote::PlaySpeakerSound(const SpeakerSound&)
{
}

JoyCon& JoyConRemote::PointingJoyCon() const
{
    if (right != nullptr)
        return *right;

    return *left;
}

ButtonMask JoyConRemote::HeldButtons() const
{
    if (right == nullptr)
        return MapButtons(left->Buttons(), LeftJoyConAloneMap);

    ButtonMask buttons = MapButtons(right->Buttons(), RightJoyConMap);
    if (left != nullptr)
        buttons |= MapButtons(left->Buttons(), LeftJoyConAsNunchukMap);

    if (pointerKind == JoyConPointer::Gyroscope)
        buttons |= CrossOfStick(right->Stick());

    return buttons;
}

void JoyConRemote::MoveHand(double elapsedSeconds)
{
    JoyCon& pointing = PointingJoyCon();
    Formats::Vector3 turn = pointing.TakeTurn();
    if (right != nullptr && left != nullptr)
        left->TakeTurn();

    if (pointerKind == JoyConPointer::Stick)
    {
        hand.Move(pointing.Stick(), elapsedSeconds);
        return;
    }

    TurnAxes axes = pointing.Side() == JoyConSide::Right ? RightJoyConAxes : LeftJoyConAxes;
    hand.Turn(turn.z * axes.rightwardSignOfZ, turn.y * axes.upwardSignOfY);
}

}
