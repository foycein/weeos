#pragma once

#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Network/NetworkConfiguration.h"

#include <cstdint>

namespace Channels
{

// The errors that the SDK's SOStartup gives a channel when the console has no network (docs/research/Channels.md section 9).
constexpr int StartupErrorNoInterface = -45;
constexpr int StartupErrorNoAddress = -76;
constexpr int StartupErrorLinkNotReady = -121;

// The number that tells which connection failed: the kind of the enabled connections and the slot of the first one.
// It is the last two digits of the support code.
constexpr int WiredConnectionNumbers = 20;
constexpr int WirelessConnectionNumbers = 30;
constexpr int UsbConnectorConnectionNumbers = 40;
constexpr int UnclearConnectionNumber = 99;

// How long SOStartup waits for a link and an address before it gives up (CHN-04, Assumed: the 45 seconds of the menu's connection test).
constexpr double ConnectionTimeOutSeconds = 45;

// What a channel finds when it tries to go online. WeeOS never connects: every try fails as on a console whose cable is unplugged
// and whose access point is out of reach (docs/DECISIONS.md), with the support code the channels show (CHN-04).
struct ConnectionFailure
{
    // What SOStartup answers: one of the StartupError numbers.
    int startupError;

    // The five digits that the Shop and the Internet Channel show, for example 50299.
    std::uint32_t supportCode;

    // How long the channel waits until it learns that the connection failed.
    double secondsUntilFailure;
};

// Asks the network configuration module to connect with the connections that the user set up in Wii Settings, as the console does,
// and tells how that fails.
ConnectionFailure TryToConnect(Ios::NetworkConfigService& network);

// 20 to 22 when only wired connections are enabled, 30 to 32 for wireless ones, 40 to 42 for connections made with the
// Nintendo Wi-Fi USB Connector (the slot of the first one is added), and 99 when none is enabled or several kinds are.
int ConnectionNumberOf(const Ios::NetworkConfiguration& configuration);

// The support code: the family of the SOStartup error plus the connection number.
std::uint32_t SupportCodeOf(int startupError, int connectionNumber);

// The code that the Forecast and the News Channel show under their message, which the WiiConnect24 module makes when its download
// cannot open a connection (CHN-04, Assumed).
std::uint32_t DownloadErrorCodeOf(int startupError);

}
