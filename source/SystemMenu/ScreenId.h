#pragma once

namespace SystemMenu
{

// The screens of the menu, with the numbers the original gives them (docs/research/SystemMenuStructure.md 3.3).
// Only the screens that are built are listed.
enum class ScreenId : int
{
    // The root of the tree of screens. It opens the first real screen.
    Launcher = 0x01,

    // The Wii Menu: the Message Board with the bottom bar and the channel grid as its children.
    MainFrame = 0x04,

    WiiSettings = 0x12,
    WiiOptionsBackground = 0x15,

    HealthAndSafety = 0x1a,
    ParentalDialog = 0x1b,
    BackToWiiMenu = 0x1e,
    LimitOver = 0x1f,
    StartupRouter = 0x20,
};

}
