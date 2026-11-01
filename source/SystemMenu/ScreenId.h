#pragma once

namespace SystemMenu
{

// The screens of the menu, with the numbers the original gives them (docs/research/SystemMenuStructure.md 3.3).
// The screens that are built are listed, and the screens that a built screen asks for.
enum class ScreenId : int
{
    // The root of the tree of screens. It opens the first real screen.
    Launcher = 0x01,

    // The channel grid, and the window that opens when a channel is pressed (not built yet).
    ChannelGrid = 0x02,
    ChannelStartWindow = 0x03,

    // The Wii Menu: the Message Board with the bottom bar and the channel grid as its children.
    MainFrame = 0x04,

    // The bar at the bottom of the Wii Menu, and the overlay that draws its page arrows over the channel grid.
    BottomBar = 0x05,
    BarOverlay = 0x06,

    // The editors of Data Management: the channels, and the save data of the Wii.
    ChannelDataEditor = 0x0c,
    WiiSaveDataEditor = 0x0d,

    WiiSettings = 0x12,

    // Wii Options is three screens: the buttons that lead to Data Management and Wii Settings, the background, and the bar with the Back button.
    WiiOptionsChoices = 0x13,
    WiiOptionsBackground = 0x15,
    WiiOptionsBar = 0x16,

    HealthAndSafety = 0x1a,
};

}
