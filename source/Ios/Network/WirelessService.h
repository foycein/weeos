#pragma once

#include "Ios/Network/MacAddress.h"
#include "Ios/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Ios
{

// What the radio of the console says about itself: the answer of GetInfo.
struct WirelessInformation
{
    MacAddress macAddress;

    // One bit for each channel, bit 1 is channel 1. The channels the radio may use where the console was sold.
    std::uint16_t enabledChannelMask;

    // The channels of the mask above that Nintendo DS games may use. A DS cannot use more than three.
    std::uint16_t nintendoDsChannelMask;

    // The country the radio is set for, for example "US".
    std::string countryCode;

    // The channel the radio starts on.
    std::uint8_t channel;

    bool isInitialized;

    // The version of the radio's driver.
    std::string versionText;
};

enum class ScanType : std::uint16_t
{
    // Listen for the beacons the access points send.
    Passive = 0,

    // Ask the access points to answer.
    Active = 1,
};

// What to search for. The channels are numbered like the bits of WirelessInformation::enabledChannelMask.
struct ScanOptions
{
    std::uint16_t channelMask = 0;
    std::uint16_t maximumChannelTimeInMilliseconds = 0;

    // The address of the access point, or all bits set for any access point.
    MacAddress bssid = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ScanType type = ScanType::Active;

    // The name of the access point, up to 32 characters, or nothing for any.
    std::string ssid;
};

// One access point that the search found.
struct AccessPointInformation
{
    MacAddress bssid;
    std::string ssid;
    std::uint8_t channel;

    // How well the radio receives it, from 0 (not at all) to 100.
    std::uint8_t signalStrength;

    // The access point wants a key.
    bool isProtected;
};

// The Wi-Fi manager (WD) of IOS, in place of /dev/net/wd/command. The menu uses two of its calls: it asks for the information
// of the radio (the Console Information page) and it searches for access points (the list of access points in the settings).
//
// The console here has no radio. It answers with the values of a console that was sold in the USA, and the search finds nothing,
// as a console does that is out of reach of every access point.
//
// All calls are thread safe, because the service keeps no state.
class WirelessService
{
public:
    explicit WirelessService(const MacAddress& wirelessMacAddress);

    WirelessInformation GetInformation() const;

    // Searches for access points. InvalidArgument when the name is longer than 32 characters.
    Result<std::vector<AccessPointInformation>> Scan(const ScanOptions& options) const;

private:
    MacAddress wirelessMacAddress;
};

}
