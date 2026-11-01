#pragma once

namespace Channels
{

// The program of one channel, written for WeeOS. The original title carries a program for the PowerPC (content 1), which the console
// starts after IOS has restarted with the title's IOS. In WeeOS the channel system makes this object instead and runs it on the same
// engines as the menu (docs/research/Channels.md 1.2, docs/audit/OpenPoints/Channels.md CHN-15).
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

    // Draws the picture of the channel, its hands included. The black of the fader is drawn over it.
    virtual void Draw() = 0;

    // False while the HOME button must do nothing, as on the Shop's page of a damaged save, which turns off the HOME Button Menu.
    virtual bool AllowsHomeButton() const
    {
        return true;
    }
};

}
