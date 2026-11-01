#pragma once

#include "SystemMenu/OwnServer.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Sockets/SocketService.h"

#include <cstdint>
#include <future>
#include <optional>
#include <string_view>

namespace SystemMenu
{

// What the connection test found. The console is online through the PC, with nothing to set up (docs/DECISIONS.md, Online).
struct ConnectionTestResult
{
    // The PC is connected, so the console is: "connected through the PC".
    bool isConnectedThroughThePc = false;

    // The owner's server answered the test the way the console's test server does.
    bool hasServerAnswered = false;

    // What the console's test shows when a part fails: the support code and the message of the menu (ipl_common.bmg) that explains
    // it. Both are 0 when everything answered.
    int supportCode = 0;
    std::uint16_t messageNumber = 0;
};

// The connection test of the status page, as the menu's test does it (0x813D8D9C and 0x813D4764): the network starts, then the
// test server answers a request for its first page with the header "X-Organization: Nintendo". The test server is the owner's
// server. While the PC is not connected the test fails at once with the code of a wired connection without a cable, 51420.
// Without an address of the server, or without an answer, it fails with the menu's code of a server that cannot be reached.
//
// The test runs beside the program, so that the page goes on while it waits; a page that is left ends it.
class PcConnectionTest
{
public:
    // Starts at once. The services must outlive the test.
    PcConnectionTest(Ios::NetworkConfigService& network, Ios::SocketService& sockets, std::string_view serverAddress);
    ~PcConnectionTest();

    PcConnectionTest(const PcConnectionTest&) = delete;
    PcConnectionTest& operator=(const PcConnectionTest&) = delete;

    // Nothing while the test runs.
    std::optional<ConnectionTestResult> Result();

private:
    ConnectionTestResult Run(Ios::NetworkConfigService& network, Ios::SocketService& sockets);

    OwnServer server;
    std::future<ConnectionTestResult> pendingResult;
    std::optional<ConnectionTestResult> result;
};

// What the test makes of the reply of the server, the HTTP test of 0x813D4764 and its callback 0x813D5304.
ConnectionTestResult JudgeServerReply(const ServerReply& reply);

}
