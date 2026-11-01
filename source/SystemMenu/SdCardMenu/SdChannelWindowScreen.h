#pragma once

#include "SystemMenu/ButtonGroup.h"
#include "SystemMenu/DataManagement/ChannelIconLayout.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

class SdChannelListScreen;

enum class SdWindowState
{
    // The window is built and asks the list for the zoom into the television of the channel.
    WaitingForTheZoom,

    // The list zooms in; the window fades in inside the television.
    ZoomingIn,

    // The icon and the name of the channel show, and the hands can press Wii Menu and Start.
    ShowingChannel,

    // The menu's message that the channel cannot be used is open.
    ShowingCannotBeUsed,

    // Wii Menu was pressed: the list zooms out, the window fades out inside the television.
    ZoomingOut,
};

// The window of a channel of the SD card (screen 0x23, sdChanTtl.ash, iplSDChannelTitle.cpp 0x813E4BBC): the icon of the channel and its
// name, with the buttons Wii Menu and Start, which opens and closes with the zoom of the list of the card like the channel start window
// with the zoom of the grid. The original starts a channel of the card as a temporary title that it copies to the NAND (IPL.TID), and
// makes room with Auto Manage first. WeeOS has no program for any channel that the card can hold, so Start shows the menu's message that
// the channel cannot be used (SYS-76).
class SdChannelWindowScreen : public Screen
{
public:
    explicit SdChannelWindowScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    SdWindowState State() const;

protected:
    void BeforeStep() override;
    bool IsReady() override;
    void OnStart() override;
    bool RunMain() override;

private:
    void ReactToButton(const ButtonGroupEvent& event);
    void PressStart();
    void CloseTowardsTheList();
    SdChannelListScreen* List();
    void DrawTheWindow();

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> frame;
    std::unique_ptr<MenuLayout> background;
    std::unique_ptr<ChannelIconLayout> icon;
    std::unique_ptr<ButtonGroup> buttons;
    std::u16string name;
    Formats::Bytes iconFile;

    std::size_t focusOnOfWiiMenu = 0;
    std::size_t focusOffOfWiiMenu = 0;
    std::size_t focusOnOfStart = 0;
    std::size_t focusOffOfStart = 0;
    std::size_t pressOfWiiMenu = 0;
    std::size_t pressOfStart = 0;

    SdWindowState state = SdWindowState::WaitingForTheZoom;
};

}
