#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"
#include "SystemMenu/WiiSettings/PcConnectionTest.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Sockets/SocketService.h"
#include "Settings/PcSettings.h"
#include "Text/MessageCatalog.h"

#include <memory>

namespace SystemMenu
{

// What the user comes back to when a connection test is over, and what the pages of the test call themselves.
struct ConnectionTestPlan
{
    PageId returnPage = PageId::InternetIndex;
    int tabDepth = 0;
    std::u16string title;
};

// The test of the connection: the page "Testing connection..." and the page with what the test found (PcConnectionTest).
// While the PC is connected the page says so, and names the code of the owner's server when the server did not answer;
// while it is not, it names the code and the explanation of a connection that cannot be made, like the original.
class ConnectionTestPages : public PageGroup
{
public:
    // The navigator, the services, the settings of the PC (the address of the server) and the messages must outlive the pages.
    ConnectionTestPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService, Ios::SocketService& socketService,
        const Settings::PcSettingsStore& pcSettingsStore, const Text::MessageCatalog& messageCatalog);

    // Starts a test and shows the page that waits for it.
    void Begin(const ConnectionTestPlan& testPlan);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

    // What the last test found. For the tests.
    ConnectionTestResult LastResult() const;

private:
    std::vector<std::u16string> LinesOfTheResult() const;

    PageNavigator& navigator;
    Ios::NetworkConfigService& networkConfig;
    Ios::SocketService& sockets;
    const Settings::PcSettingsStore& pcSettings;
    const Text::MessageCatalog& messages;

    ConnectionTestPlan plan;
    std::unique_ptr<PcConnectionTest> test;
    int stepsTested = 0;
    ConnectionTestResult result;
};

}
