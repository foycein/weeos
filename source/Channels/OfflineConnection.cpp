#include "Channels/OfflineConnection.h"

#include <optional>

namespace Channels
{

namespace
{

// The support code families of SOStartup errors (the System Menu's 0x81494C04, the same table is in the Shop and the Internet Channel).
constexpr std::uint32_t NoInterfaceFamily = 50200;
constexpr std::uint32_t LinkNotReadyFamily = 51000;
constexpr std::uint32_t NoAddressFamily = 51300;
constexpr std::uint32_t WiredLinkFamily = 51400;

// The code of the WiiConnect24 module is 100000, plus 1000 for each step of its work (7 is the download), plus 100 for each kind
// of error (2 is an error of the sockets), minus the error of the sockets.
constexpr std::uint32_t DownloadSocketErrorCodeBase = 107200;

// A bit for each slot that holds a connection of one kind.
struct EnabledConnections
{
    unsigned wired = 0;
    unsigned wireless = 0;
    unsigned usbConnector = 0;
};

EnabledConnections FindEnabledConnections(const Ios::NetworkConfiguration& configuration)
{
    EnabledConnections enabled;
    for (std::size_t slot = 0; slot < configuration.profiles.size(); ++slot)
    {
        const Ios::ConnectionProfile& profile = configuration.profiles[slot];
        unsigned slotBit = 1u << slot;
        if (!profile.isEnabled)
            continue;

        if (profile.isWired)
            enabled.wired |= slotBit;
        else if (profile.wireless.configMethod == Ios::ConfigMethodWifiUsbConnector)
            enabled.usbConnector |= slotBit;
        else
            enabled.wireless |= slotBit;
    }
    return enabled;
}

int FirstSlotOf(unsigned slotBits)
{
    int slot = 0;
    while ((slotBits & 1u) == 0)
    {
        slotBits >>= 1;
        ++slot;
    }
    return slot;
}

bool IsWiredConnectionNumber(int connectionNumber)
{
    return connectionNumber >= WiredConnectionNumbers && connectionNumber < WirelessConnectionNumbers;
}

int StartupErrorOf(Ios::LinkStatus linkStatus)
{
    if (linkStatus == Ios::LinkStatus::WiredInterfaceReady)
        return StartupErrorNoAddress;

    if (linkStatus == Ios::LinkStatus::Busy)
        return StartupErrorLinkNotReady;

    return StartupErrorNoInterface;
}

}

ConnectionFailure TryToConnect(Ios::NetworkConfigService& network)
{
    Ios::NetworkConfiguration configuration = network.GetCurrentConfiguration();
    Ios::LinkStatus linkStatus = Ios::LinkStatus::NoInterface;
    if (network.ApplyConfiguration(configuration).Succeeded())
        linkStatus = network.GetLinkStatus();

    ConnectionFailure failure;
    failure.startupError = StartupErrorOf(linkStatus);
    failure.supportCode = SupportCodeOf(failure.startupError, ConnectionNumberOf(configuration));
    failure.secondsUntilFailure = failure.startupError == StartupErrorNoInterface ? 0 : ConnectionTimeOutSeconds;
    return failure;
}

int ConnectionNumberOf(const Ios::NetworkConfiguration& configuration)
{
    EnabledConnections enabled = FindEnabledConnections(configuration);

    if (enabled.wired != 0 && enabled.wireless == 0 && enabled.usbConnector == 0)
        return WiredConnectionNumbers + FirstSlotOf(enabled.wired);

    if (enabled.wireless != 0 && enabled.wired == 0 && enabled.usbConnector == 0)
        return WirelessConnectionNumbers + FirstSlotOf(enabled.wireless);

    if (enabled.usbConnector != 0 && enabled.wired == 0 && enabled.wireless == 0)
        return UsbConnectorConnectionNumbers + FirstSlotOf(enabled.usbConnector);

    return UnclearConnectionNumber;
}

std::uint32_t SupportCodeOf(int startupError, int connectionNumber)
{
    std::uint32_t family = NoInterfaceFamily;
    if (startupError == StartupErrorLinkNotReady)
        family = LinkNotReadyFamily;

    if (startupError == StartupErrorNoAddress)
        family = NoAddressFamily;

    if (startupError != StartupErrorNoInterface && IsWiredConnectionNumber(connectionNumber))
        family = WiredLinkFamily;

    return family + static_cast<std::uint32_t>(connectionNumber);
}

std::uint32_t DownloadErrorCodeOf(int startupError)
{
    return DownloadSocketErrorCodeBase + static_cast<std::uint32_t>(-startupError);
}

}
