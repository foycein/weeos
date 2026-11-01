#include "SystemMenu/NetworkStart.h"

namespace SystemMenu
{

namespace
{

// The support code families of the SOStartup errors.
constexpr std::uint32_t NoInterfaceFamily = 50200;
constexpr std::uint32_t NoAddressFamily = 51300;
constexpr std::uint32_t WiredLinkFamily = 51400;

constexpr int FirstWiredConnectionNumber = 20;
constexpr int FirstWirelessConnectionNumber = 30;

bool IsWiredConnectionNumber(int connectionNumber)
{
    return connectionNumber >= FirstWiredConnectionNumber && connectionNumber < FirstWirelessConnectionNumber;
}

NetworkStart FailedStart(int startupError, double secondsUntilFailure)
{
    NetworkStart start;
    start.startupError = startupError;
    start.supportCode = SupportCodeOf(startupError, PcConnectionNumber);
    start.secondsUntilFailure = secondsUntilFailure;
    return start;
}

}

NetworkStart StartNetwork(Ios::NetworkConfigService& network, Ios::SocketService& sockets)
{
    if (network.ApplyConfiguration(network.GetCurrentConfiguration()).Failed())
        return FailedStart(StartupErrorNoInterface, 0);

    if (network.GetLinkStatus() != Ios::LinkStatus::LinkUp)
        return FailedStart(StartupErrorNoAddress, ConnectionTimeOutSeconds);

    sockets.Startup();
    NetworkStart start;
    start.isOnline = true;
    return start;
}

std::uint32_t SupportCodeOf(int startupError, int connectionNumber)
{
    std::uint32_t family = NoInterfaceFamily;
    if (startupError == StartupErrorNoAddress)
        family = NoAddressFamily;

    if (startupError != StartupErrorNoInterface && IsWiredConnectionNumber(connectionNumber))
        family = WiredLinkFamily;

    return family + static_cast<std::uint32_t>(connectionNumber);
}

}
