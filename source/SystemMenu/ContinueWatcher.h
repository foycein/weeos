#pragma once

#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"

#include <bitset>

namespace SystemMenu
{

// Decides when a screen that shows "Press A to continue" goes on: when a player presses A or B, when another remote connects,
// or when nobody does anything for a long time (the screens 0x1a and 0x1f of the original, docs/audit/SystemMenu.md).
class ContinueWatcher
{
public:
    // For this long after the notice began, remotes may connect and disconnect without that ending the screen.
    static constexpr double SecondsOfSettlingRemotes = 2;

    // The screen goes on by itself when nobody does anything for this long.
    static constexpr double SecondsBeforeGoingOnAlone = 60;

    // The input and the clock must stay alive as long as this object.
    ContinueWatcher(const InputManager& inputManager, const MenuClock& menuClock);

    // The notice begins now. The remotes that are connected now are the ones that were there.
    void Begin();

    // Once in every step after Begin. True when the screen should go on.
    bool HasUserConfirmed();

private:
    const InputManager& input;
    const MenuClock& clock;
    double beganSeconds = 0;

    // The remotes that were connected when the notice began, and as the screen last saw them.
    std::bitset<PlayerCount> rememberedPlayers;
};

}
