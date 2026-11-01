#pragma once

#include "Input/RemoteButtons.h"
#include "Input/RemoteState.h"
#include "Ios/Remote/RemoteService.h"

#include <array>
#include <bitset>
#include <optional>

namespace SystemMenu
{

// The menu has four players. Player 1 is remote 1, the channel 0, and so on up to player 4.
constexpr int PlayerCount = Input::RemoteChannelCount;

// What the menu knows about the remote of one player.
struct PlayerInput
{
    Input::DeviceKind deviceKind;
    Input::RemoteState state;
};

// A group of buttons, as the buttons of the Wii Remote and as the buttons of the Classic Controller.
struct ButtonSet
{
    Input::ButtonMask remoteButtons;
    Input::ButtonMask classicButtons;
};

// The buttons that say yes: A and B of the Wii Remote, A and B of the Classic Controller.
constexpr ButtonSet ConfirmButtons = {
    Input::RemoteButtons::A | Input::RemoteButtons::B,
    Input::ClassicButtons::A | Input::ClassicButtons::B,
};

// The buttons that press what the hand points at: A of the Wii Remote and A of the Classic Controller.
constexpr ButtonSet SelectButtons = {
    Input::RemoteButtons::A,
    Input::ClassicButtons::A,
};

// Reads the four remotes once in every step, the way the menu's controller update does (docs/research/Engines.md 5.2).
class InputManager
{
public:
    // A remote that sends nothing for this many reads in a row is disconnected.
    static constexpr int ReadsWithoutDataBeforeDisconnect = 60;

    // The remote service must stay alive as long as this object.
    explicit InputManager(Ios::RemoteService& remoteService);

    // Takes the new state of every remote that has one. A remote that has no device is forgotten.
    // A device that sends no new data keeps its last state, until it has sent nothing for ReadsWithoutDataBeforeDisconnect reads.
    void Poll();

    // The remote of the player as of the last Poll. Nothing while no device with a state is on the channel.
    const std::optional<PlayerInput>& Player(int playerIndex) const;

    // The players whose remotes are connected now.
    std::bitset<PlayerCount> ConnectedPlayers() const;

    // True when a connected remote has gone down with one of the buttons since the Poll before.
    bool AnyPlayerPressed(const ButtonSet& buttons) const;

    // The same for one player. False while the remote of the player is not connected.
    bool HasPlayerPressed(int playerIndex, const ButtonSet& buttons) const;

private:
    void PollPlayer(int playerIndex);
    void ForgetPlayer(int playerIndex);

    Ios::RemoteService& remoteService;
    std::array<std::optional<PlayerInput>, PlayerCount> players;
    std::array<int, PlayerCount> readsWithoutData = {};
};

}
