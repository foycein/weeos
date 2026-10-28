#include "Input/VirtualRemote.h"

#include "Platform/FitPicture.h"

#include <utility>

namespace Input
{

VirtualRemote::VirtualRemote(Platform::WindowSize initialWindowSize, Platform::ScreenShape initialScreenShape)
    : keyMap(DefaultKeyMap.begin(), DefaultKeyMap.end())
    , mouseMap(DefaultMouseMap.begin(), DefaultMouseMap.end())
    , windowSize(initialWindowSize)
    , screenShape(initialScreenShape)
{
}

void VirtualRemote::OnMouseMoved(int pixelX, int pixelY)
{
    mousePixel = {static_cast<float>(pixelX), static_cast<float>(pixelY)};
    mouseIsOverWindow = true;
}

void VirtualRemote::OnMouseLeft()
{
    mouseIsOverWindow = false;
    heldMouseButtons.clear();
}

void VirtualRemote::OnMouseButton(Platform::MouseButton button, bool isPressed)
{
    if (!isPressed)
    {
        heldMouseButtons.erase(button);
        return;
    }

    heldMouseButtons.insert(button);
    HandleButtonsPressed(ButtonsOfMouseButton(button));
}

void VirtualRemote::OnKey(int virtualKeyCode, bool isPressed)
{
    if (!isPressed)
    {
        heldKeys.erase(virtualKeyCode);
        return;
    }

    heldKeys.insert(virtualKeyCode);
    HandleButtonsPressed(ButtonsOfKey(virtualKeyCode));
}

void VirtualRemote::OnFocusLost()
{
    heldKeys.clear();
    heldMouseButtons.clear();
}

void VirtualRemote::SetWindowSize(Platform::WindowSize newWindowSize)
{
    windowSize = newWindowSize;
}

void VirtualRemote::SetScreenShape(Platform::ScreenShape newScreenShape)
{
    screenShape = newScreenShape;
}

void VirtualRemote::SetPictureAreaRule(PictureAreaRule newPictureAreaRule)
{
    pictureAreaRule = newPictureAreaRule;
}

void VirtualRemote::SetKeyMap(std::span<const KeyBinding> newKeyMap)
{
    keyMap.assign(newKeyMap.begin(), newKeyMap.end());
}

void VirtualRemote::SetMouseMap(std::span<const MouseBinding> newMouseMap)
{
    mouseMap.assign(newMouseMap.begin(), newMouseMap.end());
}

DeviceKind VirtualRemote::Probe() const
{
    if (!isConnected)
        return DeviceKind::None;

    return DeviceKind::Remote;
}

std::optional<RemoteState> VirtualRemote::ReadNewState()
{
    if (!isConnected)
        return std::nullopt;

    ButtonMask buttonsHeld = HeldButtons() | buttonsPressedSinceLastRead;

    RemoteState state;
    state.buttonsHeld = buttonsHeld;
    state.buttonsPressed = buttonsHeld & ~buttonsHeldAtLastRead;
    state.buttonsReleased = buttonsHeldAtLastRead & ~buttonsHeld;

    buttonsHeldAtLastRead = buttonsHeld;
    buttonsPressedSinceLastRead = 0;

    std::optional<Formats::Vector2> pointer = PointerOfTheMouse();
    if (pointer)
    {
        state.pointerIsValid = true;
        state.pointer = *pointer;
    }

    return state;
}

void VirtualRemote::Disconnect()
{
    isConnected = false;
    buttonsPressedSinceLastRead = 0;
    buttonsHeldAtLastRead = 0;
}

void VirtualRemote::SetSensorBarPosition(SensorBarPosition)
{
}

void VirtualRemote::SetRumble(bool shouldRumble)
{
    isRumbling = shouldRumble;
}

void VirtualRemote::SetPlayerLeds(const PlayerLeds& leds)
{
    playerLeds = leds;
}

void VirtualRemote::PlaySpeakerSound(const SpeakerSound& sound)
{
    waitingSpeakerSounds.push_back(sound);
}

bool VirtualRemote::IsRumbling() const
{
    return isRumbling;
}

const PlayerLeds& VirtualRemote::GetPlayerLeds() const
{
    return playerLeds;
}

std::vector<SpeakerSound> VirtualRemote::TakeSpeakerSounds()
{
    std::vector<SpeakerSound> sounds = std::move(waitingSpeakerSounds);
    waitingSpeakerSounds.clear();
    return sounds;
}

ButtonMask VirtualRemote::ButtonsOfKey(int virtualKeyCode) const
{
    ButtonMask buttons = 0;
    for (const KeyBinding& binding : keyMap)
    {
        if (binding.virtualKeyCode == virtualKeyCode)
            buttons |= binding.button;
    }
    return buttons;
}

ButtonMask VirtualRemote::ButtonsOfMouseButton(Platform::MouseButton mouseButton) const
{
    ButtonMask buttons = 0;
    for (const MouseBinding& binding : mouseMap)
    {
        if (binding.mouseButton == mouseButton)
            buttons |= binding.button;
    }
    return buttons;
}

ButtonMask VirtualRemote::HeldButtons() const
{
    ButtonMask buttons = 0;
    for (int virtualKeyCode : heldKeys)
        buttons |= ButtonsOfKey(virtualKeyCode);

    for (Platform::MouseButton mouseButton : heldMouseButtons)
        buttons |= ButtonsOfMouseButton(mouseButton);

    return buttons;
}

// A button that was pressed is remembered until the next read, even when it is released before that read.
// A remote that was disconnected connects again, as a real remote does when a button is pressed.
void VirtualRemote::HandleButtonsPressed(ButtonMask buttons)
{
    if (buttons == 0)
        return;

    buttonsPressedSinceLastRead |= buttons;
    isConnected = true;
}

std::optional<Formats::Vector2> VirtualRemote::PointerOfTheMouse() const
{
    if (!mouseIsOverWindow)
        return std::nullopt;

    Platform::PixelRectangle pictureArea = Platform::FitPicture(windowSize, screenShape);
    if (pictureAreaRule)
        pictureArea = pictureAreaRule(pictureArea, screenShape);

    return PixelToPointer(mousePixel, pictureArea);
}

}
