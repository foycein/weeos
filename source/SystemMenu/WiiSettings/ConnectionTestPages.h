#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Text/MessageCatalog.h"

namespace SystemMenu
{

// What the user comes back to when a connection test is over, and what the pages of the test call themselves.
struct ConnectionTestPlan
{
    PageId returnPage = PageId::InternetIndex;
    int tabDepth = 0;
    std::u16string title;
};

// The test of the connection that the console is set up with: the page "Testing connection..." and the page that tells why it failed.
//
// The console here has no network. The network configuration module answers as it does with an unplugged cable, so the test always
// fails, and the page names the reason like the original: a code that the user can give to the support, and an explanation.
// A test that passes cannot happen here, so there is no page for it.
class ConnectionTestPages : public PageGroup
{
public:
    // The navigator, the network configuration and the messages must outlive the pages.
    ConnectionTestPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService, const Text::MessageCatalog& messageCatalog);

    // Starts a test and shows the page that waits for it.
    void Begin(const ConnectionTestPlan& testPlan);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

    // The code and the explanation of the last test that failed. For the tests.
    int LastSupportCode() const;
    int LastMessageNumber() const;

private:
    void FindOutWhyTheTestFails();
    std::vector<std::u16string> LinesOfTheFailure() const;

    PageNavigator& navigator;
    Ios::NetworkConfigService& networkConfig;
    const Text::MessageCatalog& messages;

    ConnectionTestPlan plan;
    int stepsTested = 0;
    int supportCode = 0;
    int messageNumber = 0;
};

}
