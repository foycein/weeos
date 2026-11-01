#include "SystemMenu/WiiSettings/UpdatePages.h"

#include "SystemMenu/WiiSettings/MessageLines.h"
#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view Title = u"Wii System Update";

// The message of the menu that asks the user to set up the Internet first.
constexpr std::uint16_t ConfigureInternetMessage = 364;

}

UpdatePages::UpdatePages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService,
    ConnectionTestPages& connectionTestPages, const Text::MessageCatalog& messageCatalog)
    : navigator(pageNavigator)
    , networkConfig(networkConfigService)
    , connectionTest(connectionTestPages)
    , messages(messageCatalog)
{
}

bool UpdatePages::Owns(PageId page) const
{
    return page == PageId::UpdateIndex || page == PageId::UpdateNeedsConnection;
}

void UpdatePages::BuildPage(PageId page, Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));

    if (page == PageId::UpdateIndex)
    {
        builder.AddMessage({u"Connect to the Internet and", u"perform a Wii system update?"});
        builder.AddBottomLeft(u"Yes", [this] { StartUpdate(); });
        builder.AddBottomRight(u"No", [this] { navigator.GoTo(PageId::ThirdIndex); }).sound = PageSound::Back;
        return;
    }

    builder.AddMessage(LinesOfMessage(messages.Find(ConfigureInternetMessage)));
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::InternetIndex); });
}

bool UpdatePages::HasConnectionThatWasSetUp() const
{
    Ios::Result<Ios::NetworkConfiguration> stored = networkConfig.ReadConfiguration();
    if (stored.Failed())
        return false;

    const auto& profiles = stored.Value().profiles;
    return std::any_of(profiles.begin(), profiles.end(), [](const Ios::ConnectionProfile& profile) { return profile.isEnabled; });
}

void UpdatePages::StartUpdate()
{
    if (!HasConnectionThatWasSetUp())
    {
        navigator.GoTo(PageId::UpdateNeedsConnection);
        return;
    }

    ConnectionTestPlan plan;
    plan.returnPage = PageId::ThirdIndex;
    plan.tabDepth = 1;
    plan.title = std::u16string(Title);
    connectionTest.Begin(plan);
}

}
