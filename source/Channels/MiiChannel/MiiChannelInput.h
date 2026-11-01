#pragma once

#include "Channels/ChannelServices.h"

#include "Formats/Vectors.h"
#include "Input/RemoteButtons.h"

#include <optional>

namespace Channels
{

// The Mii Channel listens to the remote of player 1 only: without it the channel asks for it (message 500708).
struct PlayerOneInput
{
    // Where the remote points, in layout units of the picture. Nothing while it points away from the screen.
    std::optional<Formats::Vector2> pointer;

    Input::ButtonMask held = 0;
    Input::ButtonMask pressed = 0;
    Input::ButtonMask released = 0;

    bool IsHeld(Input::ButtonMask buttons) const
    {
        return (held & buttons) == buttons;
    }

    bool WasPressed(Input::ButtonMask button) const
    {
        return (pressed & button) != 0;
    }
};

inline PlayerOneInput ReadPlayerOne(const ChannelServices& services)
{
    constexpr int PlayerOne = 0;

    PlayerOneInput input;
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(PlayerOne);
    if (!player)
        return input;

    input.held = player->state.buttonsHeld;
    input.pressed = player->state.buttonsPressed;
    input.released = player->state.buttonsReleased;
    if (player->state.pointerIsValid)
        input.pointer = services.display.PointerToCanvas(player->state.pointer);
    return input;
}

}
