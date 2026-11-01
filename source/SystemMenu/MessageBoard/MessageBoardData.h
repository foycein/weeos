#pragma once

#include "SystemMenu/MessageBoard/MessageStore.h"
#include "SystemMenu/MessageBoard/MiiPortraits.h"

#include <cstdint>
#include <optional>

namespace SystemMenu
{

// What the Message Board keeps while the program runs, however often its screens come and go: the database, the faces of the Miis,
// the day that the board shows and the Mii that the memo writer was given by the Mii chooser.
struct MessageBoardData
{
    // The files, the device and the menu's files must outlive the data.
    MessageBoardData(NandFiles& nandFiles, const WirelessAddress& wirelessAddress, Graphics::GraphicsDevice& device, const MenuFiles& menuFiles)
        : store(nandFiles, wirelessAddress)
        , portraits(device, menuFiles)
        , nand(nandFiles)
    {
        store.CreateWhenMissing();
    }

    MessageStore store;
    MiiPortraits portraits;
    NandFiles& nand;

    // The day of the board. The board shows today whenever it comes from the channels (0x813935D4 sets it back with B_Ch).
    BoardDay shownDay;

    // The Mii that the Mii chooser (0x1C) gave back last, taken by the memo writer.
    std::optional<MiiData> chosenMii;
    bool wasMiiChooserCancelled = false;

    // The envelope of the bar was pressed: the main frame shows the board when the channel grid has ended.
    bool isComingFromTheChannels = false;

    // The random numbers of the menu (0x81343624 on the state 0x810890D4): the state becomes state * 0x19660D + 0x3C6EF35F and the
    // number is the state divided by 2 to the 32nd, from 0 up to 1. The places of new letters come from it.
    // Where the original starts the state was not read: WeeOS starts it from the time of the console when the first letter needs it.
    std::optional<std::uint32_t> randomState;

    float NextRandomNumber(const LocalTime& now)
    {
        constexpr std::uint32_t Multiplier = 0x19660D;
        constexpr std::uint32_t Increment = 0x3C6EF35F;
        constexpr double StateRange = 4294967296.0;
        if (!randomState)
            randomState = SecondsSince2000(now);
        randomState = *randomState * Multiplier + Increment;
        return static_cast<float>(*randomState / StateRange);
    }
};

}
