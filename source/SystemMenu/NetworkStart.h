#pragma once

#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Sockets/SocketService.h"

#include <cstdint>

namespace SystemMenu
{

// The errors that the SDK's SOStartup gives a program when the network cannot be started.
constexpr int StartupErrorNoInterface = -45;
constexpr int StartupErrorNoAddress = -76;

// The last two digits of a support code tell which connection failed: 20 to 22 for a wired connection in slot 1 to 3.
// The PC's connection is the console's one wired connection, in slot 1.
constexpr int PcConnectionNumber = 20;

// How long SOStartup waits for an address before it gives up (CHN-04, Assumed: the 45 seconds of the menu's connection test).
constexpr double ConnectionTimeOutSeconds = 45;

// What a program finds when it starts the network as the SDK's SOStartup does.
struct NetworkStart
{
    bool isOnline = false;

    // When it is not online: the error of SOStartup, the support code that the programs show (for example 51420), and how long the
    // program waits until it learns that the start failed.
    int startupError = 0;
    std::uint32_t supportCode = 0;
    double secondsUntilFailure = 0;
};

// Asks the network configuration module to connect, as every program does, and starts the socket stack. The console is online while
// the PC is connected; while it is not, the cable of the wired connection is out, and SOStartup waits for an address until the time-out.
NetworkStart StartNetwork(Ios::NetworkConfigService& network, Ios::SocketService& sockets);

// The support code: the family of the SOStartup error (the menu's table at 0x81494C04, which the Shop and the Internet Channel
// share) plus the connection number.
std::uint32_t SupportCodeOf(int startupError, int connectionNumber);

}
