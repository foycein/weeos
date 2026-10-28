#pragma once

#include "Input/ButtonMap.h"
#include "Input/PointerSpace.h"
#include "Input/RemoteSource.h"

#include <set>
#include <span>
#include <vector>

namespace Input
{

// Virtual Wii Remote 1. The mouse is the pointer, the mouse buttons and the keys press the buttons of the remote.
// It is connected from the start and has new data at every read, so the menu never loses it.
// The events of the window and the reads of the menu must come from the same thread.
class VirtualRemote : public RemoteSource
{
public:
    // The window size and the screen shape tell where the picture is in the window, which the pointer needs.
    VirtualRemote(WindowSize initialWindowSize, ScreenShape initialScreenShape);

    // ---- The events of the window, in the order they happen. They are the five events of the window listener of the platform part.

    // The mouse moved over the window. The position is in pixels from the top left corner of the area inside the window.
    void OnMouseMoved(int pixelX, int pixelY);

    // The mouse left the window. The mouse buttons count as released, because the window does not see them being released.
    void OnMouseLeft();

    void OnMouseButton(MouseButton button, bool isPressed);

    // The code is the Windows virtual key code.
    void OnKey(int virtualKeyCode, bool isPressed);

    // The window lost the keyboard focus. Every key and every mouse button counts as released.
    void OnFocusLost();

    // ---- What changes while the program runs

    // The program tells the remote when the window is resized and when the screen shape setting changes.
    void SetWindowSize(WindowSize newWindowSize);
    void SetScreenShape(ScreenShape newScreenShape);

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
    ButtonMask ButtonsOfMouseButton(MouseButton mouseButton) const;
    ButtonMask HeldButtons() const;
    void HandleButtonsPressed(ButtonMask buttons);
    std::optional<Formats::Vector2> PointerOfTheMouse() const;

    std::vector<KeyBinding> keyMap;
    std::vector<MouseBinding> mouseMap;

    std::set<int> heldKeys;
    std::set<MouseButton> heldMouseButtons;

    bool mouseIsOverWindow = false;
    Formats::Vector2 mousePixel = {0, 0};
    WindowSize windowSize;
    ScreenShape screenShape;

    bool isConnected = true;
    ButtonMask buttonsPressedSinceLastRead = 0;
    ButtonMask buttonsHeldAtLastRead = 0;

    bool isRumbling = false;
    PlayerLeds playerLeds = {};
    std::vector<SpeakerSound> waitingSpeakerSounds;
};

}
