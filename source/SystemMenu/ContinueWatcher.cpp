#include "SystemMenu/ContinueWatcher.h"

namespace SystemMenu
{

ContinueWatcher::ContinueWatcher(const InputManager& inputManager, const MenuClock& menuClock)
    : input(inputManager)
    , clock(menuClock)
{
}

void ContinueWatcher::Begin()
{
    beganSeconds = clock.Seconds();
    rememberedPlayers = input.ConnectedPlayers();
}

// A remote that connects counts as a press, because the user presses a button to connect it.
bool ContinueWatcher::HasUserConfirmed()
{
    std::bitset<PlayerCount> connectedPlayers = input.ConnectedPlayers();
    double secondsSinceBegin = clock.Seconds() - beganSeconds;

    if (secondsSinceBegin < SecondsOfSettlingRemotes)
        rememberedPlayers = connectedPlayers;

    bool hasNewRemote = (connectedPlayers & ~rememberedPlayers).any();

    // A remote that went away is forgotten, so that it counts as new when it comes back.
    if (!hasNewRemote)
        rememberedPlayers = connectedPlayers;

    bool isConfirmed = input.AnyPlayerPressed(ConfirmButtons);
    bool hasWaitedTooLong = secondsSinceBegin > SecondsBeforeGoingOnAlone;
    return isConfirmed || hasNewRemote || hasWaitedTooLong;
}

}
