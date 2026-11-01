#pragma once

#include "SystemMenu/WiiSettings/PageContent.h"
#include "SystemMenu/WiiSettings/PageId.h"

namespace SystemMenu
{

// The pages of one part of Wii Settings, with what they remember while the user works on them.
class PageGroup
{
public:
    virtual ~PageGroup() = default;

    virtual bool Owns(PageId page) const = 0;

    // The user comes to the page, from another page. The group takes the values that the page edits from the settings.
    virtual void EnterPage(PageId page)
    {
        static_cast<void>(page);
    }

    // Fills the page. It is called again every time something changes, so a page is made from what the group knows now.
    virtual void BuildPage(PageId page, Page& content) = 0;

    // One step of time passes while one of the pages of the group is shown, for the pages that wait for something.
    virtual void Step(PageId shownPage)
    {
        static_cast<void>(shownPage);
    }
};

}
