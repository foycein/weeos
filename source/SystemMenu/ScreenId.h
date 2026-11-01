#pragma once

namespace SystemMenu
{

// The screens of the menu, with the numbers the original gives them (docs/research/SystemMenuStructure.md 3.3).
// Only the screens that are built are listed.
enum class ScreenId : int
{
    // The root of the tree of screens. It opens the first real screen.
    Launcher = 0x01,

    // The channel grid: the pages of channels with the TV frame, the clock and the names of the channels.
    ChannelGrid = 0x02,

    // The window that opens when a channel is pressed.
    ChannelStartWindow = 0x03,

    // The Wii Menu: the Message Board with the bottom bar and the channel grid as its children.
    MainFrame = 0x04,

    // The bar at the bottom of the Wii Menu with the Wii button, the Message Board button and the page arrows.
    BottomBar = 0x05,

    // Draws the page arrows and the balloons of the bar over the channel grid.
    BarOverlay = 0x06,

    // The background of Wii Options, which the Wii button leads to.
    WiiOptionsBackground = 0x15,

    HealthAndSafety = 0x1a,
};

}
