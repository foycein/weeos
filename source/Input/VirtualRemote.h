#pragma once

#include "Input/ButtonMap.h"
#include "Input/PointerSpace.h"
#include "Input/RemoteSource.h"

#include "Platform/PixelRectangle.h"
#include "Platform/WindowListener.h"
#include "Platform/WindowSize.h"

#include <set>
#include <span>
#include <vector>

namespace Input
{

// Says which part of the screen area shows the picture, for the pointer. The rule gets the screen area, which is what
// Platform::FitPicture gives for the window, and returns the area in which the pointer is measured.
// Graphics::ConsolePictureArea is the rule of the picture that is made the way the console makes it.
using PictureAreaRule = Platform::PixelRectangle (*)(const Platform::PixelRectangle& screenArea);

// Virtual Wii Remote 1. The mouse is the pointer, the mouse buttons and the keys press the buttons of the remote.
// It is connected from the start and has new data at every read, so the menu never loses it.
// It is the listener of the window: give it to Window::SetListener. The events of the window and the reads of the menu
// must come from the same thread.
class VirtualRemote : public RemoteSource, public Platform::WindowListener
{
public:
    // The window size tells where the 16:9 picture is in the window, which the pointer needs.
    explicit VirtualRemote(Platform::WindowSize initialWindowSize);

    // ---- The events of the window, in the order they happen

    // The position is in pixels from the top left corner of the area inside the window.
    void OnMouseMoved(int pixelX, int pixelY) override;

    // The mouse buttons count as released, because the window does not see them being released outside of it.
    void OnMouseLeft() override;

    void OnMouseButton(Platform::MouseButton button, bool isPressed) override;

    // Every notch presses Plus or Minus once (WheelAwayButton, WheelTowardsButton).
    void OnMouseWheel(int notches) override;

    void OnKey(int virtualKeyCode, bool isPressed) override;

    // Every key and every mouse button counts as released.
    void OnFocusLost() override;

    // ---- What changes while the program runs

    // The program tells the remote when the window is resized.
    void SetWindowSize(Platform::WindowSize newWindowSize);

    // The pointer is measured in the whole screen area until the program gives a rule, for example when it switches the graphics
    // to the picture of the console. The rule is used for every read, so it follows the window size.
    // A null rule goes back to the whole screen area.
    void SetPictureAreaRule(PictureAreaRule newPictureAreaRule);

    // Gives other keys and mouse buttons to the buttons of the remote. A key that is down gets the meaning it has in the new table.
    void SetKeyMap(std::span<const KeyBinding> newKeyMap);
    void SetMouseMap(std::span<const MouseBinding> newMouseMap);

    // ---- RemoteSource

    DeviceKind Probe() const override;

    // The pointer is valid while the mouse is over the window and the picture has an area.
    // A button that goes down and up again between two reads is still reported, as held for one read.
    std::optional<RemoteState> ReadNewState() override;

    void Disconnect() override;

    // A mouse points at the picture itself, so the place of the sensor bar changes nothing.
    void SetSensorBarPosition(SensorBarPosition position) override;

    void SetRumble(bool shouldRumble) override;
    void SetPlayerLeds(const PlayerLeds& leds) override;

    // A mouse has no speaker. The sounds wait here until the program takes them to play them on the PC.
    void PlaySpeakerSound(const SpeakerSound& sound) override;

    // ---- What the menu asked for

    bool IsRumbling() const;
    const PlayerLeds& GetPlayerLeds() const;
    std::vector<SpeakerSound> TakeSpeakerSounds();

private:
    ButtonMask ButtonsOfKey(int virtualKeyCode) const;
    ButtonMask ButtonsOfMouseButton(Platform::MouseButton mouseButton) const;
    ButtonMask HeldButtons() const;
    void HandleButtonsPressed(ButtonMask buttons);
    std::optional<Formats::Vector2> PointerOfTheMouse() const;

    std::vector<KeyBinding> keyMap;
    std::vector<MouseBinding> mouseMap;

    std::set<int> heldKeys;
    std::set<Platform::MouseButton> heldMouseButtons;

    bool mouseIsOverWindow = false;
    Formats::Vector2 mousePixel = {0, 0};
    Platform::WindowSize windowSize;
    PictureAreaRule pictureAreaRule = nullptr;

    bool isConnected = true;
    ButtonMask buttonsPressedSinceLastRead = 0;
    ButtonMask buttonsHeldAtLastRead = 0;

    bool isRumbling = false;
    PlayerLeds playerLeds = {};
    std::vector<SpeakerSound> waitingSpeakerSounds;
};

}
