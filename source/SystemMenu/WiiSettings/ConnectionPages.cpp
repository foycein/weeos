#include "SystemMenu/WiiSettings/ConnectionPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view Title = u"Internet";

// The test shows its page for about two and a half seconds, as the connection test of a Wii does.
constexpr int StepsOfTesting = 150;

}

ConnectionPages::ConnectionPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService)
    : navigator(pageNavigator)
    , networkConfig(networkConfigService)
{
}

bool ConnectionPages::Owns(PageId page) const
{
    return page >= PageId::ConnectionStatus && page <= PageId::ConnectionTestFailed;
}

void ConnectionPages::EnterPage(PageId page)
{
    if (page == PageId::ConnectionTesting)
        stepsTested = 0;
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

    if (page == PageId::ConnectionTestPassed)
        builder.AddMessage({u"The connection test was successful.", u"", u"WeeOS reaches the internet through this PC."});
    else
        builder.AddMessage({u"The connection test failed.", u"", u"WeeOS could not reach the internet through this PC.",
            u"Check the internet connection of the PC", u"and try again."});

    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::ConnectionStatus); });
}

void ConnectionPages::BuildStatus(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage({u"Connected through the PC", u"", u"WeeOS uses the internet connection of this PC.", u"There is nothing to set up."});
    builder.AddBackButton([this] { navigator.GoTo(PageId::Index); });
    builder.AddBottomRight(u"Connection Test", [this] { navigator.GoTo(PageId::ConnectionTesting); });
}

void ConnectionPages::Step(PageId shownPage)
{
    if (shownPage != PageId::ConnectionTesting)
        return;

    ++stepsTested;
    if (stepsTested < StepsOfTesting)
        return;

    if (CanReachTheInternet())
        navigator.GoTo(PageId::ConnectionTestPassed);
    else
        navigator.GoTo(PageId::ConnectionTestFailed);
}

// The module is asked to connect, as a program that goes online does, and the link it reports is the answer.
bool ConnectionPages::CanReachTheInternet()
{
    Ios::Result<void> applied = networkConfig.ApplyConfiguration(networkConfig.GetCurrentConfiguration());
    if (applied.Failed())
        return false;

    return networkConfig.GetLinkStatus() == Ios::LinkStatus::LinkUp;
}

}
