#include "SystemMenu/WiiSettings/ConnectionTestPages.h"

#include "SystemMenu/WiiSettings/MessageLines.h"
#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <format>

namespace SystemMenu
{

namespace
{

// The test shows its page for about two and a half seconds.
constexpr int StepsOfTesting = 150;

// The messages of the menu that explain why a connection does not work (docs/audit/OpenPoints/Settings.md SET-08).
constexpr std::uint16_t ErrorCodeMessage = 400;
constexpr std::uint16_t UnableToConnectMessage = 405;
constexpr std::uint16_t NoConnectionConfiguredMessage = 406;
constexpr std::uint16_t SettingsIncorrectMessage = 407;
constexpr std::uint16_t AdapterNotInsertedMessage = 408;
constexpr std::uint16_t AccessPointNotFoundMessage = 409;

// The support codes of the console come from the function of the System Menu that turns the answers of the socket library into codes:
// a wired connection that cannot start gives 51300, a wireless one 51400 (the answers -39 and -48 of the library, 0x81494C04).
// The codes of the other two failures are Assumed.
constexpr int WiredConnectionFailedCode = 51300;
constexpr int WirelessConnectionFailedCode = 51400;
constexpr int NoConnectionConfiguredCode = 51000;
constexpr int SettingsIncorrectCode = 52000;

std::u16string NumberAsText(int number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

}

ConnectionTestPages::ConnectionTestPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService,
    const Text::MessageCatalog& messageCatalog)
    : navigator(pageNavigator)
    , networkConfig(networkConfigService)
    , messages(messageCatalog)
{
}

void ConnectionTestPages::Begin(const ConnectionTestPlan& testPlan)
{
    plan = testPlan;
    navigator.GoTo(PageId::ConnectionTesting);
}

bool ConnectionTestPages::Owns(PageId page) const
{
    return page == PageId::ConnectionTesting || page == PageId::ConnectionTestFailed;
}

void ConnectionTestPages::EnterPage(PageId page)
{
    if (page == PageId::ConnectionTesting)
        stepsTested = 0;
}

void ConnectionTestPages::BuildPage(PageId page, Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(plan.tabDepth, plan.title);

    if (page == PageId::ConnectionTesting)
    {
        builder.AddMessage({u"Testing connection..."});
        return;
    }

    builder.AddMessage(LinesOfTheFailure());
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(plan.returnPage); });
}

void ConnectionTestPages::Step(PageId shownPage)
{
    if (shownPage != PageId::ConnectionTesting)
        return;

    ++stepsTested;
    if (stepsTested < StepsOfTesting)
        return;

    FindOutWhyTheTestFails();
    navigator.GoTo(PageId::ConnectionTestFailed);
}

int ConnectionTestPages::LastSupportCode() const
{
    return supportCode;
}

int ConnectionTestPages::LastMessageNumber() const
{
    return messageNumber;
}

// The module is asked to connect with the settings that are stored. An unplugged cable gives these answers.
void ConnectionTestPages::FindOutWhyTheTestFails()
{
    Ios::Result<Ios::NetworkConfiguration> stored = networkConfig.ReadConfiguration();
    if (stored.Failed())
    {
        supportCode = NoConnectionConfiguredCode;
        messageNumber = NoConnectionConfiguredMessage;
        return;
    }

    Ios::Result<void> applied = networkConfig.ApplyConfiguration(stored.Value());
    if (applied.Failed())
    {
        supportCode = SettingsIncorrectCode;
        messageNumber = SettingsIncorrectMessage;
        return;
    }

    Ios::LinkStatus status = networkConfig.GetLinkStatus();
    if (status == Ios::LinkStatus::WiredInterfaceReady)
    {
        supportCode = WiredConnectionFailedCode;
        messageNumber = AdapterNotInsertedMessage;
    }
    else if (status == Ios::LinkStatus::Busy)
    {
        supportCode = WirelessConnectionFailedCode;
        messageNumber = AccessPointNotFoundMessage;
    }
    else if (status == Ios::LinkStatus::NoInterface)
    {
        supportCode = NoConnectionConfiguredCode;
        messageNumber = NoConnectionConfiguredMessage;
    }
    else
    {
        supportCode = WirelessConnectionFailedCode;
        messageNumber = UnableToConnectMessage;
    }
}

std::vector<std::u16string> ConnectionTestPages::LinesOfTheFailure() const
{
    std::vector<std::u16string> lines = {messages.Find(ErrorCodeMessage) + NumberAsText(supportCode), u""};

    std::vector<std::u16string> explanation = LinesOfMessage(messages.Find(static_cast<std::uint16_t>(messageNumber)));
    lines.insert(lines.end(), explanation.begin(), explanation.end());
    return lines;
}

}
