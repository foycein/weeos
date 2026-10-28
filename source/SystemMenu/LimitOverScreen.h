#pragma once

#include "SystemMenu/ContinueWatcher.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

// The message that shows when the menu starts after a title that had a time limit ran out (screen 0x1f). It fades in, and a second
// later "Press A to continue" starts to blink. The screen goes on like Health and Safety does, fades out and gives way to the Wii Menu.
// The record of the title in /shared2/expired is deleted when the screen loads.
class LimitOverScreen : public Screen
{
public:
    explicit LimitOverScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

protected:
    bool IsReady() override;
    bool RunMain() override;
    bool RunClosing() override;

private:
    void ShowOnlyTheEnglishMessage();
    void FitFramePicturesToWideScreen();

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    Formats::Pane* noticePicture = nullptr;

    std::size_t fadeInSlot = 0;
    std::size_t noticeSlot = 0;
    std::size_t fadeOutSlot = 0;

    bool hasFadeInStarted = false;
    bool hasMessageFadedIn = false;
    double noticeWaitBeganSeconds = 0;

    ContinueWatcher continueWatcher;
};

}
