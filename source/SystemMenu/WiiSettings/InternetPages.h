#pragma once

#include "SystemMenu/WiiSettings/ConnectionTestPages.h"
#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Network/WirelessService.h"

#include <functional>
#include <optional>
#include <vector>

namespace SystemMenu
{

// "Internet": the three connections of the console, how to set one up, and what the console knows about itself.
//
// The console has no network: the module of the network configuration answers like an unplugged cable and the search for access
// points finds nothing. A wired connection can be set up but its test fails; a wireless one can only be set up when the search finds
// an access point, which it does not, so the page that says so is what the user sees. The manual setup of a wireless connection
// (name, security, addresses) is not built, and neither is the page "User Agreements", whose agreement is the one of WiiConnect24
// and the Wii Shop Channel (docs/audit/OpenPoints/Settings.md SET-09 and SET-10).
class InternetPages : public PageGroup
{
public:
    // The search for access points gives the access points that the radio found. The things that are given must outlive the pages.
    using AccessPointSearch = std::function<std::vector<Ios::AccessPointInformation>()>;

    InternetPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService, ConnectionTestPages& connectionTestPages,
        AccessPointSearch accessPointSearch, std::function<Ios::MacAddress()> wirelessMacAddress);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

private:
    void BuildInternetIndex(Page& content);
    void BuildConnectionList(Page& content);
    void BuildConnectionKind(Page& content);
    void BuildWirelessSetup(Page& content);
    void BuildAccessPointSearch(Page& content);
    void BuildNoAccessPointFound(Page& content);
    void BuildAccessPointList(Page& content);
    void BuildSecurityKeyEntry(Page& content);
    void BuildConnectionSaved(Page& content);
    void BuildUsbConnectorIntroduction(Page& content);
    void BuildUsbConnectorWaiting(Page& content);
    void BuildAossWaiting(Page& content);
    void BuildConnectionActions(Page& content);
    void BuildClearConnectionConfirm(Page& content);
    void BuildConnectionInUse(Page& content);
    void BuildConnectionTestStart(Page& content);
    void BuildConsoleInformation(Page& content);

    Ios::NetworkConfiguration LoadConfiguration() const;
    void StoreConfiguration(const Ios::NetworkConfiguration& configuration);

    void ChooseConnection(int profileNumber);
    void SetUpWiredConnection();
    void SetUpWirelessConnection(const Ios::AccessPointInformation& accessPoint, const std::string& key);
    void UseConnection();
    void ClearConnection();
    void StartConnectionTest();
    void AskForSecurityKey();

    // The title of the pages of one connection: "Connection 1".
    std::u16string ConnectionTitle() const;

    PageNavigator& navigator;
    Ios::NetworkConfigService& networkConfig;
    ConnectionTestPages& connectionTest;
    AccessPointSearch searchForAccessPoints;
    std::function<Ios::MacAddress()> readWirelessMacAddress;

    // The connection the user works on, counted from 0.
    int profileNumber = 0;

    // The access points of the last search, the page of the list that is shown, the one that was chosen and the key typed for it.
    std::vector<Ios::AccessPointInformation> foundAccessPoints;
    int shownAccessPointPage = 0;
    std::optional<Ios::AccessPointInformation> chosenAccessPoint;
    std::string typedKey;

    int stepsWaited = 0;
};

}
