#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Screen": where the picture lies, the shape of the screen, the kind of picture and the burn-in reduction.
//
// The position and the shape show their result at once, in the menu itself, so the pages change the settings while the user works
// and put the old values back when the user goes back. The other two pages change the settings when the user confirms.
class DisplayPages : public PageGroup
{
public:
    // The navigator and the settings must outlive the pages.
    DisplayPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildDisplayIndex(Page& content);
    void BuildScreenPosition(Page& content);
    void BuildWidescreenSet(Page& content);
    void BuildTvResolution(Page& content);
    void BuildBurnInReduction(Page& content);

    void MovePicture(int change);
    void ChooseAspectRatio(Settings::AspectRatio aspectRatio);

    // The pages that show their result at once are left with the old values back in the settings, or with the new values written down.
    void LeaveScreenPositionWithoutConfirming();
    void LeaveWidescreenWithoutConfirming();
    void SaveAndGoBackToTheDisplayIndex();

    PageNavigator& navigator;
    Settings::SystemSettings& settings;

    // What the settings held when the user came to the page.
    int displayOffsetAtEntry = 0;
    Settings::AspectRatio aspectRatioAtEntry = Settings::AspectRatio::SixteenByNine;

    // What the user has chosen on a page that changes the settings when the user confirms.
    bool isProgressiveChosen = false;
    bool isScreenSaverChosen = false;
};

}
