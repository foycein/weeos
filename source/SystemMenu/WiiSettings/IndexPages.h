#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

namespace SystemMenu
{

// The three pages that lead to all the others: "Wii System Settings 1", "2" and "3".
class IndexPages : public PageGroup
{
public:
    // The navigator must outlive the pages.
    explicit IndexPages(PageNavigator& pageNavigator);

    bool Owns(PageId page) const override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildFirstIndex(Page& content);
    void BuildSecondIndex(Page& content);
    void BuildThirdIndex(Page& content);

    PageNavigator& navigator;
};

}
