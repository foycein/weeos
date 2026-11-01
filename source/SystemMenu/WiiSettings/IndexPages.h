#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

namespace SystemMenu
{

// "Wii System Settings": the page that leads to the four parts of Wii Settings that WeeOS has (docs/DECISIONS.md, Settings):
// Console Nickname, Sensor Bar, Internet and PC Settings. A Wii has three such pages; the others held what follows the PC in WeeOS.
class IndexPages : public PageGroup
{
public:
    // The navigator must outlive the page.
    explicit IndexPages(PageNavigator& pageNavigator);

    bool Owns(PageId page) const override;
    void BuildPage(PageId page, Page& content) override;

private:
    PageNavigator& navigator;
};

}
