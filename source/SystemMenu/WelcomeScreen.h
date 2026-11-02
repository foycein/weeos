#pragma once

#include "SystemMenu/ContinueWatcher.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiSettings/PageFont.h"

#include <memory>
#include <optional>

namespace SystemMenu
{

// The first screen of a cold start (screen 0x1a): where a Wii shows its health and safety warning, WeeOS welcomes the user. In the calm
// white and blue of the Wii it says what WeeOS is, who made it, its version and a friendly word about breaks, with our own words drawn
// by our own text code, and "Press A" below.
//
// The flow is the one of the original's warning: the welcome fades in, a second later "Press A" starts to blink, and the screen goes on
// when a player presses A or B, when another remote connects, or after 60 seconds. Then the welcome fades out, the picture fades to
// black and the Wii Menu is asked for.
class WelcomeScreen : public Screen
{
public:
    explicit WelcomeScreen(MenuServices& menuServices);

    void Build() override;
    void Draw(DrawPass pass) override;

    // The screen is drawn also when a screen lies over it.
    bool DrawsWithChildren() const override;

    // How opaque the welcome is now, from 0 to 255, and "Press A": nothing until it shows.
    int WelcomeOpacity() const;
    std::optional<int> NoticeOpacity() const;

protected:
    bool IsReady() override;
    bool RunMain() override;
    bool RunClosing() override;

private:
    void DrawBackground(int opacity);
    void DrawLogoLine(int opacity);
    void DrawText(PageFont& font, std::u16string_view text, const PageRectangle& bounds, float size, Formats::Rgba8Color color, int opacity);

    MenuServices& services;

    // The logo is drawn with the large font of the menu, which is sharp at its size but has only the letters the menu needs; the texts
    // with the small font, which has every letter.
    std::unique_ptr<PageFont> logoFont;
    std::unique_ptr<PageFont> textFont;

    // The steps since the fade in or the fade out began, and the steps since "Press A" began.
    int stepsOfFadeIn = 0;
    int stepsOfFadeOut = 0;
    bool isFadingOut = false;
    bool isNoticeShown = false;
    int stepsOfNotice = 0;

    // The welcome has faded in and the wait for the notice began at this time.
    bool hasFadedIn = false;
    double noticeWaitBeganSeconds = 0;

    ContinueWatcher continueWatcher;
};

}
