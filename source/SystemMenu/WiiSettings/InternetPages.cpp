#include "SystemMenu/WiiSettings/InternetPages.h"

#include "SystemMenu/MenuError.h"
#include "SystemMenu/WiiSettings/NumberText.h"
#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <algorithm>
#include <format>
#include <utility>

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view InternetTitle = u"Internet";
constexpr std::u16string_view ConnectionSettingsTitle = u"Connection Settings";
constexpr std::u16string_view ConsoleInformationTitle = u"Console Information";

constexpr int ConnectionCount = static_cast<int>(Ios::ConnectionProfileCount);

// The page "Searching for an access point..." stays for about two and a half seconds, and the wait for AOSS for a minute
// (a step lasts 1001/60000 of a second).
constexpr int StepsOfSearching = 150;
constexpr int StepsOfWaitingForAoss = 3597;

// The list of access points has five rows on a page, like the list of countries.
constexpr int AccessPointsOnPage = 5;
constexpr int ListLeft = 88;
constexpr int ListTop = 76;
constexpr int RowWidth = 432;
constexpr int RowHeight = 56;
constexpr int ListArrowLeft = 528;
constexpr int ListArrowUpTop = 76;
constexpr int ListArrowDownTop = 282;
constexpr int ArrowSize = 72;

// The picture of the strength of the signal has four levels, 0 to 3, for the strengths 0 to 100.
constexpr int SignalIconSize = 40;
constexpr int StrengthPerSignalLevel = 25;
constexpr int HighestSignalLevel = 3;

// A connection in the list: the name in its left half and the kind of connection in its right half.
constexpr int ConnectionNameOffset = 10;
constexpr int ConnectionNameWidth = 190;
constexpr int ConnectionKindOffset = 200;
constexpr int ConnectionKindWidth = 190;
constexpr int ConnectionRowHeight = 70;
constexpr int ConnectionSlotCount = 3;
constexpr int ConnectionFirstTop = 85;
constexpr int ConnectionRowDistance = 96;
constexpr int ConnectionLeft = 107;

constexpr std::size_t MaximumKeyLength = 63;

constexpr PageRectangle MacAddressLabelBounds = {24, 90, 560, 40};
constexpr PageRectangle MacAddressBounds = {24, 130, 560, 60};
constexpr PageRectangle AdapterLabelBounds = {24, 230, 560, 40};

std::u16string ConnectionKindText(const Ios::ConnectionProfile& profile)
{
    if (!profile.isEnabled)
        return u"None";

    if (profile.isWired)
        return u"Wired";

    return u"Wireless";
}

Ios::SelectedMedia MediaOf(const Ios::ConnectionProfile& profile)
{
    if (profile.isWired)
        return Ios::SelectedMedia::Wired;

    return Ios::SelectedMedia::Wireless;
}

// The connection in use is the first one of the kind that the console connects with.
std::optional<int> FindConnectionInUse(const Ios::NetworkConfiguration& configuration)
{
    if (configuration.selectedMedia == Ios::SelectedMedia::None)
        return std::nullopt;

    for (int number = 0; number < ConnectionCount; ++number)
    {
        const Ios::ConnectionProfile& profile = configuration.profiles[number];
        if (profile.isEnabled && MediaOf(profile) == configuration.selectedMedia)
            return number;
    }

    return std::nullopt;
}

// The first connection that is set up and is of this kind.
int FindFirstConnectionOfKind(const Ios::NetworkConfiguration& configuration, Ios::SelectedMedia media)
{
    for (int number = 0; number < ConnectionCount; ++number)
    {
        const Ios::ConnectionProfile& profile = configuration.profiles[number];
        if (profile.isEnabled && MediaOf(profile) == media)
            return number;
    }

    return ConnectionCount;
}

std::u16string MacAddressText(const Ios::MacAddress& address)
{
    std::string text;
    for (std::size_t byteNumber = 0; byteNumber < address.size(); ++byteNumber)
    {
        if (byteNumber > 0)
            text += ':';

        text += std::format("{:02X}", address[byteNumber]);
    }

    return std::u16string(text.begin(), text.end());
}

int SignalLevelOf(const Ios::AccessPointInformation& accessPoint)
{
    return std::min(accessPoint.signalStrength / StrengthPerSignalLevel, HighestSignalLevel);
}

std::u16string TextOfName(const std::string& name)
{
    return std::u16string(name.begin(), name.end());
}

}

InternetPages::InternetPages(PageNavigator& pageNavigator, Ios::NetworkConfigService& networkConfigService,
    ConnectionTestPages& connectionTestPages, AccessPointSearch accessPointSearch)
    : navigator(pageNavigator)
    , networkConfig(networkConfigService)
    , connectionTest(connectionTestPages)
    , searchForAccessPoints(std::move(accessPointSearch))
{
}

bool InternetPages::Owns(PageId page) const
{
    switch (page)
    {
    case PageId::InternetIndex:
    case PageId::ConnectionList:
    case PageId::ConnectionKind:
    case PageId::WirelessSetup:
    case PageId::AccessPointSearch:
    case PageId::NoAccessPointFound:
    case PageId::AccessPointList:
    case PageId::SecurityKeyEntry:
    case PageId::ConnectionSaved:
    case PageId::UsbConnectorIntroduction:
    case PageId::UsbConnectorWaiting:
    case PageId::AossWaiting:
    case PageId::ConnectionActions:
    case PageId::ClearConnectionConfirm:
    case PageId::ConnectionInUse:
    case PageId::ConnectionTestStart:
    case PageId::ConsoleInformation:
        return true;

    default:
        return false;
    }
}

void InternetPages::EnterPage(PageId page)
{
    if (page == PageId::AccessPointSearch || page == PageId::AossWaiting)
        stepsWaited = 0;

    if (page == PageId::AccessPointList)
        shownAccessPointPage = 0;

    if (page == PageId::SecurityKeyEntry)
        typedKey.clear();
}

void InternetPages::BuildPage(PageId page, Page& content)
{
    switch (page)
    {
    case PageId::InternetIndex:
        BuildInternetIndex(content);
        break;

    case PageId::ConnectionList:
        BuildConnectionList(content);
        break;

    case PageId::ConnectionKind:
        BuildConnectionKind(content);
        break;

    case PageId::WirelessSetup:
        BuildWirelessSetup(content);
        break;

    case PageId::AccessPointSearch:
        BuildAccessPointSearch(content);
        break;

    case PageId::NoAccessPointFound:
        BuildNoAccessPointFound(content);
        break;

    case PageId::AccessPointList:
        BuildAccessPointList(content);
        break;

    case PageId::SecurityKeyEntry:
        BuildSecurityKeyEntry(content);
        break;

    case PageId::ConnectionSaved:
        BuildConnectionSaved(content);
        break;

    case PageId::UsbConnectorIntroduction:
        BuildUsbConnectorIntroduction(content);
        break;

    case PageId::UsbConnectorWaiting:
        BuildUsbConnectorWaiting(content);
        break;

    case PageId::AossWaiting:
        BuildAossWaiting(content);
        break;

    case PageId::ConnectionActions:
        BuildConnectionActions(content);
        break;

    case PageId::ClearConnectionConfirm:
        BuildClearConnectionConfirm(content);
        break;

    case PageId::ConnectionInUse:
        BuildConnectionInUse(content);
        break;

    case PageId::ConnectionTestStart:
        BuildConnectionTestStart(content);
        break;

    default:
        BuildConsoleInformation(content);
        break;
    }
}

void InternetPages::Step(PageId shownPage)
{
    if (shownPage == PageId::AccessPointSearch)
    {
        ++stepsWaited;
        if (stepsWaited < StepsOfSearching)
            return;

        foundAccessPoints = searchForAccessPoints();
        navigator.GoTo(foundAccessPoints.empty() ? PageId::NoAccessPointFound : PageId::AccessPointList);
    }
    else if (shownPage == PageId::AossWaiting)
    {
        ++stepsWaited;
        if (stepsWaited >= StepsOfWaitingForAoss)
            navigator.GoTo(PageId::WirelessSetup);
    }
}

void InternetPages::BuildInternetIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(InternetTitle));

    builder.AddChoice(0, 2, u"Connection Settings", false, [this] { navigator.GoTo(PageId::ConnectionList); }).sound = PageSound::Decide;
    builder.AddChoice(1, 2, u"Console Information", false, [this] { navigator.GoTo(PageId::ConsoleInformation); }).sound = PageSound::Decide;
    builder.AddBackButton([this] { navigator.GoTo(PageId::SecondIndex); });
}

void InternetPages::BuildConnectionList(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, std::u16string(ConnectionSettingsTitle));

    Ios::NetworkConfiguration configuration = LoadConfiguration();
    std::optional<int> connectionInUse = FindConnectionInUse(configuration);

    for (int number = 0; number < ConnectionCount; ++number)
    {
        int top = ConnectionFirstTop + number * ConnectionRowDistance;
        PageButton& button = builder.AddChoice(number, ConnectionSlotCount, u"", connectionInUse == number, [this, number] { ChooseConnection(number); });
        button.name = std::format("Connection {}", number + 1);

        builder.AddText({ConnectionLeft + ConnectionNameOffset, top, ConnectionNameWidth, ConnectionRowHeight}, TextRole::Label,
            {u"Connection " + NumberText(number + 1)});
        builder.AddText({ConnectionLeft + ConnectionKindOffset, top, ConnectionKindWidth, ConnectionRowHeight}, TextRole::Label,
            {ConnectionKindText(configuration.profiles[number])});
    }

    builder.AddBackButton([this] { navigator.GoTo(PageId::InternetIndex); });
}

void InternetPages::BuildConnectionKind(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());

    builder.AddChoice(0, 2, u"Wireless Connection", false, [this] { navigator.GoTo(PageId::WirelessSetup); }).sound = PageSound::Decide;
    builder.AddChoice(1, 2, u"Wired Connection", false, [this] { SetUpWiredConnection(); }).sound = PageSound::Decide;
    builder.AddBackButton([this] { navigator.GoTo(PageId::ConnectionList); });
}

void InternetPages::BuildWirelessSetup(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());

    builder.AddMenuItem(0, u"Search for an Access Point", [this] { navigator.GoTo(PageId::AccessPointSearch); });
    builder.AddMenuItem(1, u"Nintendo Wi-Fi USB Connector", [this] { navigator.GoTo(PageId::UsbConnectorIntroduction); });
    builder.AddMenuItem(2, u"AOSS", [this] { navigator.GoTo(PageId::AossWaiting); });
    builder.AddBackButton([this] { navigator.GoTo(PageId::ConnectionKind); });
}

void InternetPages::BuildAccessPointSearch(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Searching for an access point..."});
}

void InternetPages::BuildNoAccessPointFound(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"No access point was found."});
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::WirelessSetup); });
}

// The access points of the search, five on a page, each with a picture of the strength of its signal. The right button searches again.
void InternetPages::BuildAccessPointList(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());

    int accessPointCount = static_cast<int>(foundAccessPoints.size());
    int firstAccessPoint = shownAccessPointPage * AccessPointsOnPage;
    int lastAccessPoint = std::min(firstAccessPoint + AccessPointsOnPage, accessPointCount);

    for (int number = firstAccessPoint; number < lastAccessPoint; ++number)
    {
        const Ios::AccessPointInformation& accessPoint = foundAccessPoints[number];
        PageRectangle bounds = {ListLeft, ListTop + (number - firstAccessPoint) * RowHeight, RowWidth, RowHeight};

        PageButton& row = builder.AddPictureButton(ButtonLook::CountryItem, bounds, "AccessPoint " + accessPoint.ssid,
            [this, number]
            {
                chosenAccessPoint = foundAccessPoints[number];
                navigator.GoTo(chosenAccessPoint->isProtected ? PageId::SecurityKeyEntry : PageId::ConnectionSaved);
            });
        row.text = TextOfName(accessPoint.ssid);

        PageRectangle iconBounds = {bounds.left + bounds.width - SignalIconSize - 12, bounds.top + (RowHeight - SignalIconSize) / 2, SignalIconSize,
            SignalIconSize};
        builder.AddPicture("FIX/COMMON/List_Icon/WiFi_Denpa_0" + std::to_string(SignalLevelOf(accessPoint)) + ".gif", iconBounds);
    }

    if (shownAccessPointPage > 0)
    {
        builder.AddPictureButton(ButtonLook::ArrowUp, {ListArrowLeft, ListArrowUpTop, ArrowSize, ArrowSize}, "PreviousAccessPoints",
            [this]
            {
                --shownAccessPointPage;
                navigator.Refresh();
            }).sound = PageSound::Scroll;
    }

    if (lastAccessPoint < accessPointCount)
    {
        builder.AddPictureButton(ButtonLook::ArrowDown, {ListArrowLeft, ListArrowDownTop, ArrowSize, ArrowSize}, "NextAccessPoints",
            [this]
            {
                ++shownAccessPointPage;
                navigator.Refresh();
            }).sound = PageSound::Scroll;
    }

    builder.AddBackButton([this] { navigator.GoTo(PageId::WirelessSetup); });
    builder.AddBottomRight(u"Try again", [this] { navigator.GoTo(PageId::AccessPointSearch); });
}

// The access point is protected: the user types the password or the key.
void InternetPages::BuildSecurityKeyEntry(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());

    builder.AddText({24, 90, 560, 60}, TextRole::Message, {u"Input the password or key."});
    builder.AddChoice(0, 1, TextOfName(typedKey), false, [this] { AskForSecurityKey(); }).name = "Key";

    builder.AddBackButton([this] { navigator.GoTo(PageId::AccessPointList); });
    builder.AddBottomRight(u"OK",
        [this]
        {
            if (chosenAccessPoint && !typedKey.empty())
                SetUpWirelessConnection(*chosenAccessPoint, typedKey);
        });
}

void InternetPages::BuildConnectionSaved(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Settings have been saved.", u"Initiating connection test."});

    builder.AddBottomRight(u"OK", [this] { StartConnectionTest(); });
}

void InternetPages::BuildUsbConnectorIntroduction(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Install the Nintendo Wi-Fi USB", u"Connector software on your PC", u"and then insert the Nintendo",
        u"Wi-Fi USB Connector into your", u"PC's USB port. Choose Next to", u"continue."});

    builder.AddBackButton([this] { navigator.GoTo(PageId::WirelessSetup); });
    builder.AddBottomRight(u"Next", [this] { navigator.GoTo(PageId::UsbConnectorWaiting); });
}

// The adapter never answers here: the page waits until the user cancels.
void InternetPages::BuildUsbConnectorWaiting(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Use the Nintendo Wi-Fi", u"USB Connector registration", u"tool on your PC to grant this", u"Wii permission to connect."});

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::UsbConnectorIntroduction); }).sound = PageSound::Back;
}

void InternetPages::BuildAossWaiting(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Confirm that you are using an", u"AOSS-compatible access point.", u"Then, press the AOSS button on", u"the access point until",
        u"the AOSS lamp flashes. This", u"process may take up to one", u"minute."});

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::WirelessSetup); }).sound = PageSound::Back;
}

void InternetPages::BuildConnectionActions(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());

    builder.AddMenuItem(0, u"Use this connection",
        [this]
        {
            UseConnection();
            navigator.GoTo(PageId::ConnectionInUse);
        });
    builder.AddMenuItem(1, u"Connection test", [this] { navigator.GoTo(PageId::ConnectionTestStart); });
    builder.AddMenuItem(2, u"Clear settings", [this] { navigator.GoTo(PageId::ClearConnectionConfirm); });
    builder.AddBackButton([this] { navigator.GoTo(PageId::ConnectionList); });
}

void InternetPages::BuildClearConnectionConfirm(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"These settings can't be recovered", u"once they have been erased. Are", u"you sure you want to continue?"});

    builder.AddBottomLeft(u"No", [this] { navigator.GoTo(PageId::ConnectionActions); }).sound = PageSound::Back;
    builder.AddBottomRight(u"Yes", [this] { ClearConnection(); });
}

void InternetPages::BuildConnectionInUse(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Use Connection " + NumberText(profileNumber + 1) + u"."});

    builder.AddBackButton([this] { navigator.GoTo(PageId::ConnectionActions); });
}

void InternetPages::BuildConnectionTestStart(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(3, ConnectionTitle());
    builder.AddMessage({u"Initiating connection test."});

    builder.AddBottomRight(u"OK", [this] { StartConnectionTest(); });
}

void InternetPages::BuildConsoleInformation(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, std::u16string(ConsoleInformationTitle));

    builder.AddText(MacAddressLabelBounds, TextRole::Message, {u"MAC Address"});
    builder.AddText(MacAddressBounds, TextRole::Message, {MacAddressText(networkConfig.GetWirelessMacAddress())});

    // The console has no LAN adapter, so the line is dim, as it is on a console without one.
    builder.AddText(AdapterLabelBounds, TextRole::Label, {u"LAN Adapter MAC Address"});

    builder.AddBackButton([this] { navigator.GoTo(PageId::InternetIndex); });
}

Ios::NetworkConfiguration InternetPages::LoadConfiguration() const
{
    Ios::Result<Ios::NetworkConfiguration> stored = networkConfig.ReadConfiguration();
    if (stored.Failed())
        return Ios::NetworkConfiguration();

    return stored.Value();
}

void InternetPages::StoreConfiguration(const Ios::NetworkConfiguration& configuration)
{
    Ios::Result<void> written = networkConfig.WriteConfiguration(configuration);
    if (written.Failed())
        throw MenuError(std::format("The network settings cannot be saved (result code {})", Ios::OriginalResultCode(written.Error())));
}

// A connection that is set up opens the actions for it, one that is not opens the choice of the kind.
void InternetPages::ChooseConnection(int number)
{
    profileNumber = number;
    Ios::NetworkConfiguration configuration = LoadConfiguration();
    navigator.GoTo(configuration.profiles[number].isEnabled ? PageId::ConnectionActions : PageId::ConnectionKind);
}

// The wired connection asks for its addresses from the router, as a console does that is set up by the page.
void InternetPages::SetUpWiredConnection()
{
    Ios::NetworkConfiguration configuration = LoadConfiguration();

    Ios::ConnectionProfile profile;
    profile.isEnabled = true;
    profile.isWired = true;
    profile.usesDhcpForAddress = true;
    profile.usesDhcpForDns = true;
    configuration.profiles[profileNumber] = profile;

    StoreConfiguration(configuration);
    UseConnection();
    navigator.GoTo(PageId::ConnectionSaved);
}

void InternetPages::SetUpWirelessConnection(const Ios::AccessPointInformation& accessPoint, const std::string& key)
{
    Ios::NetworkConfiguration configuration = LoadConfiguration();

    Ios::ConnectionProfile profile;
    profile.isEnabled = true;
    profile.usesDhcpForAddress = true;
    profile.usesDhcpForDns = true;
    profile.wireless.configMethod = Ios::ConfigMethodSearch;
    profile.wireless.ssid = accessPoint.ssid;
    profile.wireless.encryption = Ios::EncryptionWpa2Aes;
    profile.wireless.keyLengthOrWepKeyNumber = static_cast<std::uint16_t>(key.size());
    std::copy_n(key.begin(), std::min(key.size(), profile.wireless.key.size()), profile.wireless.key.begin());
    configuration.profiles[profileNumber] = profile;

    StoreConfiguration(configuration);
    UseConnection();
    navigator.GoTo(PageId::ConnectionSaved);
}

// The connection in use is the first one of its kind, so the chosen connection takes the place of the first one.
void InternetPages::UseConnection()
{
    Ios::NetworkConfiguration configuration = LoadConfiguration();
    Ios::SelectedMedia media = MediaOf(configuration.profiles[profileNumber]);

    int firstOfKind = FindFirstConnectionOfKind(configuration, media);
    if (firstOfKind < profileNumber)
        std::swap(configuration.profiles[profileNumber], configuration.profiles[firstOfKind]);

    configuration.selectedMedia = media;
    StoreConfiguration(configuration);
    profileNumber = firstOfKind;
}

void InternetPages::ClearConnection()
{
    Ios::NetworkConfiguration configuration = LoadConfiguration();
    configuration.profiles[profileNumber] = Ios::ConnectionProfile();

    // The console connects with a kind that still has a connection, or with nothing.
    if (!FindConnectionInUse(configuration))
    {
        configuration.selectedMedia = Ios::SelectedMedia::None;
        for (const Ios::ConnectionProfile& profile : configuration.profiles)
        {
            if (profile.isEnabled)
            {
                configuration.selectedMedia = MediaOf(profile);
                break;
            }
        }
    }

    StoreConfiguration(configuration);
    navigator.GoTo(PageId::ConnectionList);
}

void InternetPages::StartConnectionTest()
{
    ConnectionTestPlan plan;
    plan.returnPage = PageId::ConnectionActions;
    plan.tabDepth = 3;
    plan.title = ConnectionTitle();
    connectionTest.Begin(plan);
}

void InternetPages::AskForSecurityKey()
{
    InputRequest request;
    request.kind = InputKind::Text;
    request.initialText = TextOfName(typedKey);
    request.maximumLength = MaximumKeyLength;

    navigator.RequestInput(request,
        [this](std::optional<std::u16string> answer)
        {
            if (answer)
                typedKey = NameOfText(*answer);

            navigator.Refresh();
        });
}

std::u16string InternetPages::ConnectionTitle() const
{
    return u"Connection " + NumberText(profileNumber + 1);
}

}
