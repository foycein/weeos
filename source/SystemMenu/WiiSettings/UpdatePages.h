#pragma once

#include "SystemMenu/WiiSettings/ConnectionTestPages.h"
#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Text/MessageCatalog.h"

namespace SystemMenu
{

// "Wii System Update": the question, and what happens when the user says yes. The update itself needs the Internet, and the console is
// offline (docs/DECISIONS.md): without a connection that was set up the user is sent to the Internet settings, with one the connection
// is tested and the test fails like an unplugged cable.
class UpdatePages : public PageGroup
{
public:
    // The navigator, the network configuration, the connection test and the messages must outlive the pages.
    UpdatePages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService, ConnectionTestPages& connectionTestPages,
        const Text::MessageCatalog& messageCatalog);

    bool Owns(PageId page) const override;
    void BuildPage(PageId page, Page& content) override;

private:
    bool HasConnectionThatWasSetUp() const;
    void StartUpdate();

    PageNavigator& navigator;
    Ios::NetworkConfigService& networkConfig;
    ConnectionTestPages& connectionTest;
    const Text::MessageCatalog& messages;
};

}
