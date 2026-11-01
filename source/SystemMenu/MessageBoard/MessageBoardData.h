#pragma once

#include "SystemMenu/MessageBoard/MessageStore.h"
#include "SystemMenu/MessageBoard/MiiPortraits.h"

#include <optional>
#include <random>

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

    // The random places of new letters.
    std::mt19937 randomNumbers{std::random_device{}()};

    float NextRandomNumber()
    {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(randomNumbers);
    }
};

}
