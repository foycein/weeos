#include "Ios/Network/WirelessService.h"

#include "Ios/Network/NetworkConfiguration.h"

namespace Ios
{

namespace
{

// The radio of the console reports these values. Nothing on the NAND records them, so they are the values of a console
// that was sold in the USA: the channels 1 to 11, and for the Nintendo DS the channels 1 and 7.
constexpr std::uint16_t UsaEnabledChannelMask = 0x0FFE;
constexpr std::uint16_t UsaNintendoDsChannelMask = 0x0082;
constexpr std::string_view UsaCountryCode = "US";

// The radio starts on the first channel from channel 3 on that it may use.
constexpr std::uint8_t StartChannel = 3;

constexpr std::string_view DriverVersionText = "Ver.4.30.47.3";

}

WirelessService::WirelessService(const MacAddress& macAddress)
    : wirelessMacAddress(macAddress)
{
}

WirelessInformation WirelessService::GetInformation() const
{
    WirelessInformation information;
    information.macAddress = wirelessMacAddress;
    information.enabledChannelMask = UsaEnabledChannelMask;
    information.nintendoDsChannelMask = UsaNintendoDsChannelMask;
    information.countryCode = UsaCountryCode;
    information.channel = StartChannel;
    information.isInitialized = true;
    information.versionText = DriverVersionText;
    return information;
}

Result<std::vector<AccessPointInformation>> WirelessService::Scan(const ScanOptions& options) const
{
    if (options.ssid.size() > MaximumSsidLength)
        return IosError::InvalidArgument;

    return std::vector<AccessPointInformation>();
}

}
