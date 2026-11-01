#include "SystemMenu/WiiSettings/ConnectionPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <string>

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view Title = u"Internet";

// The test shows its page for about two and a half seconds at least, as the connection test of a Wii does.
constexpr int StepsOfTesting = 150;

std::u16string ErrorCodeLine(int supportCode)
{
    std::string digits = std::to_string(supportCode);
    return u"Error Code: " + std::u16string(digits.begin(), digits.end());
}

}

ConnectionPages::ConnectionPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService,
    Ios::SocketService& socketService, const Settings::PcSettingsStore& pcSettingsStore)
    : navigator(pageNavigator)
    , networkConfig(networkConfigService)
    , sockets(socketService)
    , pcSettings(pcSettingsStore)
{
}

bool ConnectionPages::Owns(PageId page) const
{
    return page >= PageId::ConnectionStatus && page <= PageId::ConnectionTestFailed;
}

void ConnectionPages::EnterPage(PageId page)
{
    if (page != PageId::ConnectionTesting)
        return;

    stepsTested = 0;
    test = std::make_unique<PcConnectionTest>(networkConfig, sockets, pcSettings.Values().serverAddress);
}

void ConnectionPages::BuildPage(PageId page, Page& content)
{
    if (page == PageId::ConnectionStatus)
    {
        BuildStatus(content);
        return;
    }

    PageBuilder builder(content);
    builder.SetTitle(2, u"Connection Test");

    if (page == PageId::ConnectionTesting)
    {
        builder.AddMessage({u"Testing the connection..."});
        return;
    }

    builder.AddMessage(LinesOfTheResult());
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::ConnectionStatus); });
}

// The test passes while the PC is connected. The owner's server comes later (docs/DECISIONS.md, Online), so the page only names
// its code when it does not answer.
std::vector<std::u16string> ConnectionPages::LinesOfTheResult() const
{
    if (!result.isConnectedThroughThePc)
    {
        return {u"The connection test failed.", u"", u"WeeOS could not reach the internet through this PC.",
            u"Check the internet connection of the PC", u"and try again.", u"", ErrorCodeLine(result.supportCode)};
    }

    std::vector<std::u16string> lines = {u"The connection test was successful.", u"", u"WeeOS reaches the internet through this PC.", u""};
    if (result.hasServerAnswered)
        lines.push_back(u"The server answered the connection test.");
    else
        lines.insert(lines.end(), {u"The server could not be reached.", ErrorCodeLine(result.supportCode)});

    return lines;
}

void ConnectionPages::BuildStatus(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage(
        {u"Connected through the PC", u"", u"WeeOS uses the internet connection of this PC.", u"There is nothing to set up."});
    builder.AddBackButton([this] { navigator.GoTo(PageId::Index); });
    builder.AddBottomRight(u"Connection Test", [this] { navigator.GoTo(PageId::ConnectionTesting); });
}

void ConnectionPages::Step(PageId shownPage)
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
    if (result.isConnectedThroughThePc)
        navigator.GoTo(PageId::ConnectionTestPassed);
    else
        navigator.GoTo(PageId::ConnectionTestFailed);
}

ConnectionTestResult ConnectionPages::LastResult() const
{
    return result;
}

}
