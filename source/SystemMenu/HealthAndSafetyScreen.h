#pragma once

#include "SystemMenu/ContinueWatcher.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

// The first screen of a cold start (screen 0x1a): the health and safety warning, and below it "Press A to continue".
//
// The warning fades in. A second after that the notice below it starts to blink. The screen goes on when a player presses A or B,
// when another remote connects, or after 60 seconds. Then the warning fades out, the picture fades to black and the Wii Menu is asked for.
// The pictures are text in the original's files; WeeOS puts its own words there later (see PicturesOfTheScreen).
class HealthAndSafetyScreen : public Screen
{
public:
    explicit HealthAndSafetyScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The screen is drawn also when a screen lies over it.
    bool DrawsWithChildren() const override;

protected:
    bool IsReady() override;
    bool RunMain() override;
    bool RunClosing() override;

private:
    // The source of the pictures of the layout. A later pass replaces it with a source that holds WeeOS's own pictures
    // under the names it_Has_a_US_ENG.tpl and it_Has_b_US_ENG.tpl.
    const Layout::PictureSource& PicturesOfTheScreen() const;

    void ShowOnlyTheEnglishWarning();
    void NoteWhenTheWarningHasFadedIn();
    void StartNotice();

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    Formats::Pane* noticePicture = nullptr;

    std::size_t seenInSlot = 0;
    std::size_t noticeSlot = 0;
    std::size_t seenOutSlot = 0;

    // The warning has faded in and the wait for the notice began at this time.
    bool hasWarningFadedIn = false;
    double noticeWaitBeganSeconds = 0;

    ContinueWatcher continueWatcher;
};

}
