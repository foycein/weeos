#pragma once

#include "Channels/EffectHands.h"

#include "SystemMenu/CursorManager.h"

namespace Channels
{

// The program of one channel, written for WeeOS. The original title carries a program for the PowerPC (content 1), which the console
// starts after IOS has restarted with the title's IOS. In WeeOS the channel system makes this object instead and runs it on the same
// engines as the menu.
//
// A channel reads its files from the contents of its own title, draws its picture, plays its sounds by name and ends by asking for
// the Wii Menu (ChannelServices).
class Channel
{
public:
    Channel() = default;
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    virtual ~Channel() = default;

    // One step of the channel, 1001/60000 of a second.
    virtual void Update() = 0;

    // Draws the picture of the channel. The hands, the HOME menu and the black of the fader are drawn over it.
    virtual void Draw() = 0;

    // The hands of the four players, from the channel's own files. The channel system moves and draws them, and the HOME menu
    // draws them while it is open.
    virtual SystemMenu::CursorManager& Hands() = 0;

    // The channel's own hands when they are particle effects (the Forecast, the News and the Mii Channel), or null. The
    // channel system moves them and draws them over the channel while the HOME menu is closed; the HOME menu shows the hands of
    // Hands().
    virtual EffectHands* OwnHands()
    {
        return nullptr;
    }

    // False while the HOME button must do nothing, as on the Shop's page of a damaged save, which turns off the HOME Button Menu.
    virtual bool AllowsHomeButton() const
    {
        return true;
    }
};

}
