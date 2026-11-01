#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Ios/Network/NetworkConfigService.h"

namespace SystemMenu
{

// "Internet": WeeOS is online through the internet connection of the PC, with nothing to set up (docs/DECISIONS.md, Online). The page says
// so and offers the connection test, which asks the network configuration module of IOS to connect and shows what it reports.
class ConnectionPages : public PageGroup
{
public:
    // The navigator and the network configuration must outlive the pages.
    ConnectionPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

private:
    void BuildStatus(Page& content);
    bool CanReachTheInternet();

    PageNavigator& navigator;
    Ios::NetworkConfigService& networkConfig;
    int stepsTested = 0;
};

}
