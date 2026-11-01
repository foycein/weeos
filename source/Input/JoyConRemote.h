#pragma once

#include "Input/ControllerSettings.h"
#include "Input/JoyCon.h"
#include "Input/RemoteSource.h"
#include "Input/StickPointer.h"

#include <array>

namespace Input
{

// While the Joy-Con button is down, the button of the remote is down.
struct JoyConBinding
{
    std::uint32_t joyConButton;
    ButtonMask button;
};

// The right Joy-Con held like a Wii Remote: A is A, the trigger ZR under the finger is B like the trigger of the remote, X and B
// below the stick are 1 and 2 in the order of the remote, Y is -, + and HOME are + and HOME. A choice: there is nothing to compare with.
inline constexpr auto RightJoyConMap = std::to_array<JoyConBinding>({
    {JoyConButtons::A, RemoteButtons::A},
    {JoyConButtons::ZR, RemoteButtons::B},
    {JoyConButtons::X, RemoteButtons::One},
    {JoyConButtons::B, RemoteButtons::Two},
    {JoyConButtons::Y, RemoteButtons::Minus},
    {JoyConButtons::Plus, RemoteButtons::Plus},
    {JoyConButtons::Home, RemoteButtons::Home},
});

// The left Joy-Con beside a right one is the Nunchuk: its arrows are the cross of the remote, its - is -, ZL is Z and L is C.
inline constexpr auto LeftJoyConAsNunchukMap = std::to_array<JoyConBinding>({
    {JoyConButtons::Up, RemoteButtons::Up},
    {JoyConButtons::Down, RemoteButtons::Down},
    {JoyConButtons::Left, RemoteButtons::Left},
    {JoyConButtons::Right, RemoteButtons::Right},
    {JoyConButtons::Minus, RemoteButtons::Minus},
    {JoyConButtons::ZL, RemoteButtons::NunchukZ},
    {JoyConButtons::L, RemoteButtons::NunchukC},
});

// A left Joy-Con alone is a Wii Remote of its own: ZL is A, L is B, the side buttons SL and SR are 1 and 2, the stick click +,
// - is -, the capture button HOME.
inline constexpr auto LeftJoyConAloneMap = std::to_array<JoyConBinding>({
    {JoyConButtons::Up, RemoteButtons::Up},
    {JoyConButtons::Down, RemoteButtons::Down},
    {JoyConButtons::Left, RemoteButtons::Left},
    {JoyConButtons::Right, RemoteButtons::Right},
    {JoyConButtons::ZL, RemoteButtons::A},
    {JoyConButtons::L, RemoteButtons::B},
    {JoyConButtons::LeftSL, RemoteButtons::One},
    {JoyConButtons::LeftSR, RemoteButtons::Two},
    {JoyConButtons::LeftStickClick, RemoteButtons::Plus},
    {JoyConButtons::Minus, RemoteButtons::Minus},
    {JoyConButtons::Capture, RemoteButtons::Home},
});

// One Wii Remote made of Joy-Cons: a right one with a left one as its Nunchuk, or one Joy-Con alone. The hand follows the turns of the
// right Joy-Con (or of the one alone), or its stick, as the settings say. While the hand follows the turns, the stick of the right
// Joy-Con is the cross. After Disconnect it is not connected until a mapped button goes down.
class JoyConRemote final : public RemoteSource
{
public:
    // Either may be null, not both. The Joy-Cons must outlive this object.
    JoyConRemote(JoyCon* rightJoyCon, JoyCon* leftJoyCon);

    JoyCon* Right() const;
    JoyCon* Left() const;

    // Once in every step, after the Joy-Cons were polled, with the seconds since the step before.
    void Update(double elapsedSeconds);

    void SetPointer(JoyConPointer pointer);
    void SetPointerSpeed(int speed);

    DeviceKind Probe() const override;
    std::optional<RemoteState> ReadNewState() override;
    void Disconnect() override;
    void SetSensorBarPosition(SensorBarPosition position) override;
    void SetRumble(bool shouldRumble) override;
    void SetPlayerLeds(const PlayerLeds& leds) override;
    void PlaySpeakerSound(const SpeakerSound& sound) override;

private:
    JoyCon& PointingJoyCon() const;
    ButtonMask HeldButtons() const;
    void MoveHand(double elapsedSeconds);

    JoyCon* right;
    JoyCon* left;
    JoyConPointer pointerKind = JoyConPointer::Gyroscope;
    StickPointer hand;

    bool isAsleep = false;
    bool hasNewData = false;
    ButtonMask buttonsOfLastUpdate = 0;
    ButtonMask buttonsHeldAtLastRead = 0;
};

}
