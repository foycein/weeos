#include "SystemMenu/WiiSettings/ConnectionTestPages.h"

#include "SystemMenu/WiiSettings/MessageLines.h"
#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

// The test shows its page for about two and a half seconds at least.
constexpr int StepsOfTesting = 150;

constexpr std::uint16_t ErrorCodeMessage = 400;

// WeeOS's own words: the console is online through the PC (docs/DECISIONS.md, Online).
constexpr std::u16string_view ConnectedThroughThePcText = u"Connected through the PC.";
constexpr std::u16string_view ServerAnsweredText = u"The server answered the connection test.";

std::u16string NumberAsText(int number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

}

ConnectionTestPages::ConnectionTestPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService,
    Ios::SocketService& socketService, const Settings::PcSettingsStore& pcSettingsStore, const Text::MessageCatalog& messageCatalog)
    : navigator(pageNavigator)
    , networkConfig(networkConfigService)
    , sockets(socketService)
    , pcSettings(pcSettingsStore)
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
    if (page != PageId::ConnectionTesting)
        return;

    stepsTested = 0;
    test = std::make_unique<PcConnectionTest>(networkConfig, sockets, pcSettings.Values().serverAddress);
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

    builder.AddMessage(LinesOfTheResult());
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(plan.returnPage); });
}

void ConnectionTestPages::Step(PageId shownPage)
{
    if (shownPage != PageId::ConnectionTesting || !test)
        return;

    ++stepsTested;
    if (stepsTested < StepsOfTesting)
        return;

    std::optional<ConnectionTestResult> found = test->Result();
    if (!found)
        return;

    result = *found;
    test.reset();
    navigator.GoTo(PageId::ConnectionTestFailed);
}

ConnectionTestResult ConnectionTestPages::LastResult() const
{
    return result;
}

std::vector<std::u16string> ConnectionTestPages::LinesOfTheResult() const
{
    std::vector<std::u16string> lines;
    if (result.isConnectedThroughThePc)
        lines = {std::u16string(ConnectedThroughThePcText), u""};

    if (result.hasServerAnswered)
    {
        lines.push_back(std::u16string(ServerAnsweredText));
        return lines;
    }

    lines.push_back(messages.Find(ErrorCodeMessage) + NumberAsText(result.supportCode));
    lines.push_back(u"");
    std::vector<std::u16string> explanation = LinesOfMessage(messages.Find(result.messageNumber));
    lines.insert(lines.end(), explanation.begin(), explanation.end());
    return lines;
}

}
