#include "SystemMenu/WiiSettings/PcConnectionTest.h"

#include "SystemMenu/NetworkStart.h"

#include <cctype>
#include <chrono>
#include <string>

namespace SystemMenu
{

namespace
{

// The families of the HTTP test (0x813D4764 and 0x813D5304): the name of the test server is not found, or any other failure of
// the request or of the reply. The connection number is added, as for the start of the network.
constexpr int NameNotFoundFamily = 52100;
constexpr int RequestFailedFamily = 52200;

constexpr int HttpOk = 200;
constexpr std::string_view TestPagePath = "/";
constexpr std::string_view OrganizationHeaderName = "x-organization";
constexpr std::string_view TestServerOrganization = "Nintendo";

// The messages of the menu that explain a failed test (ipl_common.bmg).
constexpr std::uint16_t UnableToConnectToTheServerMessage = 401;
constexpr std::uint16_t UnableToConnectToTheInternetMessage = 405;

std::string LowerCase(std::string_view text)
{
    std::string lower(text);
    for (char& letter : lower)
        letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));

    return lower;
}

std::string_view Trim(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);

    while (!text.empty() && (text.back() == ' ' || text.back() == '\r'))
        text.remove_suffix(1);

    return text;
}

// The header has the line "X-Organization: Nintendo"; the name of a header line may be written in any case.
bool NamesTheTestServerOrganization(std::string_view header)
{
    std::size_t lineStart = 0;
    while (lineStart < header.size())
    {
        std::size_t lineEnd = header.find('\n', lineStart);
        if (lineEnd == std::string_view::npos)
            lineEnd = header.size();

        std::string_view line = header.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        std::size_t colon = line.find(':');
        if (colon == std::string_view::npos)
            continue;

        bool isOrganization = LowerCase(Trim(line.substr(0, colon))) == OrganizationHeaderName;
        if (isOrganization && Trim(line.substr(colon + 1)) == TestServerOrganization)
            return true;
    }
    return false;
}

ConnectionTestResult FailedTest(int family)
{
    ConnectionTestResult result;
    result.isConnectedThroughThePc = true;
    result.supportCode = family + PcConnectionNumber;
    result.messageNumber = UnableToConnectToTheServerMessage;
    return result;
}

}

ConnectionTestResult JudgeServerReply(const ServerReply& reply)
{
    if (reply.answer == ServerAnswer::NameNotFound)
        return FailedTest(NameNotFoundFamily);

    bool isAnsweredWell = reply.answer == ServerAnswer::Answered && reply.statusCode == HttpOk;
    bool isTheTestServer = isAnsweredWell && NamesTheTestServerOrganization(reply.header);
    if (!isTheTestServer)
        return FailedTest(RequestFailedFamily);

    ConnectionTestResult result;
    result.isConnectedThroughThePc = true;
    result.hasServerAnswered = true;
    return result;
}

PcConnectionTest::PcConnectionTest(Ios::NetworkConfigService& network, Ios::SocketService& sockets, std::string_view serverAddress)
    : server(sockets, serverAddress)
    , pendingResult(std::async(std::launch::async, [this, &network, &sockets] { return Run(network, sockets); }))
{
}

PcConnectionTest::~PcConnectionTest()
{
    server.Cancel();
}

// The PC knows at once whether it is connected, so the test does not wait for the time-out of SOStartup.
ConnectionTestResult PcConnectionTest::Run(Ios::NetworkConfigService& network, Ios::SocketService& sockets)
{
    NetworkStart start = StartNetwork(network, sockets);
    if (!start.isOnline)
    {
        ConnectionTestResult offline;
        offline.supportCode = static_cast<int>(start.supportCode);
        offline.messageNumber = UnableToConnectToTheInternetMessage;
        return offline;
    }

    return JudgeServerReply(server.Ask(TestPagePath));
}

std::optional<ConnectionTestResult> PcConnectionTest::Result()
{
    if (result)
        return result;

    if (pendingResult.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;

    result = pendingResult.get();
    return result;
}

}
