#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiSettings/PageFont.h"
#include "SystemMenu/WiiSettings/PageImages.h"
#include "SystemMenu/WiiSettings/PagePainter.h"
#include "SystemMenu/WiiSettings/SettingsFlow.h"

#include <memory>
#include <optional>

namespace SystemMenu
{

// Wii Settings for the screen factory. A menu that runs without the settings of the console (the tests of the other screens) has none.
std::unique_ptr<Screen> MakeWiiSettingsScreen(MenuServices& services, int argument);

// Wii Settings (screen 0x12). In the original it is a small web site shown by a browser. Here the pages are made by the SettingsFlow and drawn
// with the pictures of the original's pages; this screen brings the pointer and the buttons of the remote to the flow, and the flow's pages to the picture.
class SettingsScreen : public Screen
{
public:
    // The argument says which page comes first: 0 the first index page, 1 Internet, 3 Wii System Update, 4 the second index page,
    // 6 Country (docs/research/SystemMenuStructure.md 3.4).
    // Throws a MenuError when the menu has no settings to work with.
    SettingsScreen(MenuServices& menuServices, int argument);

    void Build() override;
    void Draw(DrawPass pass) override;

    // The settings can be seen together with a dialog that lies over them.
    bool DrawsWithChildren() const override;

    // The flow that the screen runs. For the tests and for the program that answers the requests of the pages.
    SettingsFlow& Flow();

protected:
    bool RunMain() override;
    bool RunClosing() override;

private:
    static PageId FirstPageOf(int argument);

    bool IsWaitingForAnswer();
    void OpenKeyboard(const InputRequest& request);
    void OpenParentalDialog();
    void TakeAnswerOfParentalDialog();
    void RequestSoundsOfThePages();
    void FadeMusicOnTheSoundPage(PageId pageBefore);
    void HandlePlayers();
    void PressWithPlayer(int player, std::size_t button);
    void RepeatHeldButton();
    std::optional<std::size_t> FindButtonAt(float pageX, float pageY) const;

    MenuServices& services;
    int argument;

    std::unique_ptr<PageImages> images;
    std::unique_ptr<PageFont> font;
    std::unique_ptr<PagePainter> painter;
    std::unique_ptr<SettingsFlow> flow;

    // The button that the pointer of the first remote that points at the screen is on.
    std::optional<std::size_t> pointedButton;

    bool isParentalDialogOpen = false;

    // The player who pressed a button last: the soft keyboard follows that player's pointer.
    int pressingPlayer = 0;

    // The button that is held down, by which player, and for how many steps.
    std::optional<std::size_t> heldButton;
    int heldPlayer = 0;
    int stepsHeld = 0;
};

}
