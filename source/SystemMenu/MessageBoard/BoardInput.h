#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"

#include "Formats/Vectors.h"

#include <optional>

namespace SystemMenu
{

// What the screens of the Message Board ask the remotes.

// A is the button that presses, of the Wii Remote and of the Classic Controller.
constexpr ButtonSet BoardPressButtons = {Input::RemoteButtons::A, Input::ClassicButtons::A};

// Plus and Minus of the remote, and on the Classic Controller also R and L, turn to the next and the previous day or month
// (the masks 0x6000010 and 0x30001000 of 0x8139E4EC).
constexpr ButtonSet BoardNextButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus | Input::ClassicButtons::RightTrigger};
constexpr ButtonSet BoardPreviousButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus | Input::ClassicButtons::LeftTrigger};

// The D-pad scrolls a letter.
constexpr ButtonSet ScrollUpButtons = {Input::RemoteButtons::Up, Input::ClassicButtons::Up};
constexpr ButtonSet ScrollDownButtons = {Input::RemoteButtons::Down, Input::ClassicButtons::Down};

inline bool HasGoneDown(const PlayerInput& player, const ButtonSet& buttons)
{
    bool remoteWentDown = (player.state.buttonsPressed & buttons.remoteButtons) != 0;
    bool classicWentDown = (player.state.classic.buttonsPressed & buttons.classicButtons) != 0;
    return remoteWentDown || classicWentDown;
}

inline bool IsHeldDown(const PlayerInput& player, const ButtonSet& buttons)
{
    bool remoteHeld = (player.state.buttonsHeld & buttons.remoteButtons) != 0;
    bool classicHeld = (player.state.classic.buttonsHeld & buttons.classicButtons) != 0;
    return remoteHeld || classicHeld;
}

// Where the pointer of the player is in the picture, in layout units. Nothing while the remote points elsewhere.
inline std::optional<Formats::Vector2> PointerPlaceOf(const InputManager& input, const Display& display, int playerIndex)
{
    const std::optional<PlayerInput>& player = input.Player(playerIndex);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;
    return display.PointerToCanvas(player->state.pointer);
}

}
