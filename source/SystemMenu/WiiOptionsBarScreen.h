#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

// The bar at the top and the bottom of Wii Options (screen 0x16), with the Back button and the Wii icon.
//
// The bar slides in, and the Back button takes the focus of a pointer and flashes when it is pressed. The screen with the buttons
// (WiiOptionsChoicesScreen) asks it whether the button was pressed, and shows and hides the Wii icon while an editor is open.
class WiiOptionsBarScreen : public Screen
{
public:
    explicit WiiOptionsBarScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // Looks at the pointers of the players. Gives back true once after the Back button has been pressed.
    // Nothing is looked at before the bar has slid in.
    bool WasBackButtonPressed();

    // The flash of the Back button is still going on.
    bool IsBackButtonFlashing() const;

    // The icon on the right of the bar fades in and out.
    void ShowWiiIcon();
    void HideWiiIcon();

protected:
    bool IsReady() override;
    void AfterStep() override;

private:
    void HandleButtonEvents(const std::vector<ButtonEvent>& events);

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<ButtonController> buttons;

    std::size_t slideInSlot = 0;
    std::size_t focusInSlot = 0;
    std::size_t focusOutSlot = 0;
    std::size_t flashSlot = 0;
    std::size_t iconAppearSlot = 0;
    std::size_t iconLostSlot = 0;

    bool hasStartedSlidingIn = false;
    bool isBackButtonPressed = false;
};

}
