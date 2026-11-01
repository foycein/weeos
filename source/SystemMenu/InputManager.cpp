#include "SystemMenu/InputManager.h"

namespace SystemMenu
{

namespace
{

bool HasPressed(const PlayerInput& player, const ButtonSet& buttons)
{
    if ((player.state.buttonsPressed & buttons.remoteButtons) != 0)
        return true;

    bool hasClassicController = player.deviceKind == Input::DeviceKind::RemoteWithClassicController;
    return hasClassicController && (player.state.classic.buttonsPressed & buttons.classicButtons) != 0;
}

}

InputManager::InputManager(Ios::RemoteService& service)
    : remoteService(service)
{
}

void InputManager::Poll()
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
        PollPlayer(playerIndex);
}

void InputManager::PollPlayer(int playerIndex)
{
    Input::DeviceKind deviceKind = remoteService.Probe(playerIndex);
    if (deviceKind == Input::DeviceKind::None)
    {
        ForgetPlayer(playerIndex);
        return;
    }

    std::optional<Input::RemoteState> newState = remoteService.ReadNewState(playerIndex);
    if (newState)
    {
        players[playerIndex] = PlayerInput{deviceKind, *newState};
        readsWithoutData[playerIndex] = 0;
        return;
    }

    ++readsWithoutData[playerIndex];
    if (readsWithoutData[playerIndex] > ReadsWithoutDataBeforeDisconnect)
    {
        remoteService.Disconnect(playerIndex);
        ForgetPlayer(playerIndex);
    }
}

void InputManager::ForgetPlayer(int playerIndex)
{
    players[playerIndex].reset();
    readsWithoutData[playerIndex] = 0;
}

const std::optional<PlayerInput>& InputManager::Player(int playerIndex) const
{
    return players.at(playerIndex);
}

std::bitset<PlayerCount> InputManager::ConnectedPlayers() const
{
    std::bitset<PlayerCount> connectedPlayers;
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
        connectedPlayers[playerIndex] = players[playerIndex].has_value();

    return connectedPlayers;
}

bool InputManager::AnyPlayerPressed(const ButtonSet& buttons) const
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        if (HasPlayerPressed(playerIndex, buttons))
            return true;
    }
    return false;
}

bool InputManager::HasPlayerPressed(int playerIndex, const ButtonSet& buttons) const
{
    const std::optional<PlayerInput>& player = players.at(playerIndex);
    return player && HasPressed(*player, buttons);
}

}
