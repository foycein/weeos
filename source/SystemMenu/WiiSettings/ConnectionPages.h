#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"
#include "SystemMenu/WiiSettings/PcConnectionTest.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Sockets/SocketService.h"
#include "Settings/PcSettings.h"

#include <memory>

namespace SystemMenu
{

// "Internet": WeeOS is online through the internet connection of the PC, with nothing to set up. The page says
// so and offers the connection test (PcConnectionTest): it passes while the PC is connected, and says whether WeeOS's own server answered.
class ConnectionPages : public PageGroup
{
public:
    // The navigator, the services and the settings of the PC (the address of the server) must outlive the pages.
    ConnectionPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService, Ios::SocketService& socketService,
        const Settings::PcSettingsStore& pcSettingsStore);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

    // What the last test found. For the tests.
    ConnectionTestResult LastResult() const;

private:
    void BuildStatus(Page& content);
    std::vector<std::u16string> LinesOfTheResult() const;

    PageNavigator& navigator;
    Ios::NetworkConfigService& networkConfig;
    Ios::SocketService& sockets;
    const Settings::PcSettingsStore& pcSettings;

    std::unique_ptr<PcConnectionTest> test;
    int stepsTested = 0;
    ConnectionTestResult result;
};

}
