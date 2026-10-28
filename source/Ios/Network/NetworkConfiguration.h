#pragma once

#include "Formats/Bytes.h"
#include "Ios/Network/Ipv4Address.h"
#include "Ios/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Ios
{

// The network settings of the console are kept in the file /shared2/sys/net/02/config.dat.
// It holds three connections, the ones you set up in Wii Settings under Internet.

constexpr std::size_t NetworkConfigurationFileSize = 0x1B5C;
constexpr std::size_t ConnectionProfileCount = 3;

constexpr std::size_t MaximumProxyServerNameLength = 255;
constexpr std::size_t MaximumProxyUserNameLength = 32;
constexpr std::size_t MaximumProxyPasswordLength = 32;
constexpr std::size_t MaximumSsidLength = 32;
constexpr std::size_t MaximumKeySize = 64;
constexpr std::size_t WirelessExtensionSize = 236;

// What the console connects with.
enum class SelectedMedia : std::uint8_t
{
    None = 0,
    Wireless = 1,
    Wired = 2,
};

// How the wireless connection is protected: the values of WirelessSettings::encryption.
constexpr std::uint16_t EncryptionOpen = 0;
constexpr std::uint16_t EncryptionWep64 = 1;
constexpr std::uint16_t EncryptionWep128 = 2;
constexpr std::uint16_t EncryptionWpaTkip = 4;
constexpr std::uint16_t EncryptionWpa2Aes = 5;
constexpr std::uint16_t EncryptionWpaAes = 6;

// How the wireless connection was set up: the values of WirelessSettings::configMethod.
constexpr std::uint8_t ConfigMethodManual = 0;
constexpr std::uint8_t ConfigMethodWifiUsbConnector = 1;
constexpr std::uint8_t ConfigMethodSeveralSecurities = 2;
constexpr std::uint8_t ConfigMethodSearch = 3;

struct ProxySettings
{
    bool isUsed = false;
    bool usesCredentials = false;
    std::string serverName;
    std::uint16_t port = 0;
    std::string userName;
    std::string password;

    bool operator==(const ProxySettings&) const = default;
};

// The last 348 bytes of a connection: everything about the wireless network.
struct WirelessSettings
{
    std::uint16_t rateSet = 0;
    std::uint8_t configMethod = 0;
    std::uint8_t byteAfterConfigMethod = 0;

    // The name of the access point. Its length in the file is the length of this text.
    std::string ssid;

    std::uint16_t encryption = 0;

    // The length of the key for the protections that use a passphrase, or the number of the key (0 to 3) for WEP.
    std::uint16_t keyLengthOrWepKeyNumber = 0;
    std::uint16_t wordAfterKeyLength = 0;

    // The key as the file keeps it. WEP repeats the key four times.
    std::array<std::uint8_t, MaximumKeySize> key{};

    // What the connection methods with several securities keep behind the key.
    std::array<std::uint8_t, WirelessExtensionSize> extension{};

    bool operator==(const WirelessSettings&) const = default;
};

struct ConnectionProfile
{
    // The first byte of the connection, bit by bit. A connection that is enabled takes part when the console connects.
    bool isEnabled = false;
    bool usesPathMtuRecovery = false;
    bool hasPassedConnectionTest = false;
    bool usesProxy = false;
    bool usesPppoe = false;
    bool usesDhcpForDns = false;
    bool usesDhcpForAddress = false;
    bool isWired = false;

    Ipv4Address address = 0;
    Ipv4Address netmask = 0;
    Ipv4Address gateway = 0;
    Ipv4Address primaryDns = 0;
    Ipv4Address secondaryDns = 0;

    // 0 for the default, else 68 to 1500.
    std::uint16_t mtu = 0;
    std::uint32_t tcpTimeout = 0;

    ProxySettings httpProxy;
    ProxySettings secureProxy;
    WirelessSettings wireless;

    bool operator==(const ConnectionProfile&) const = default;
};

struct NetworkConfiguration
{
    // Must be zero.
    std::uint32_t version = 0;

    SelectedMedia selectedMedia = SelectedMedia::None;

    // The permission of WiiConnect24 that the module keeps in the file. WeeOS has no WiiConnect24, so nothing sets it; it stays in
    // the configuration so that the file comes back byte for byte. The module wants all bits above the three lowest to be zero.
    std::uint8_t wiiConnect24Permission = 0;

    std::array<ConnectionProfile, ConnectionProfileCount> profiles;

    bool operator==(const NetworkConfiguration&) const = default;
};

// Reads the bytes of the file. StorageFailure when the file does not have the size of the format.
Result<NetworkConfiguration> DecodeNetworkConfiguration(Formats::ByteView fileBytes);

// Makes the bytes of the file. A text that is longer than its field is a mistake in the program and throws.
// The configuration that has nothing set gives a file of zeros, which is also what the console reads from a missing file.
Formats::Bytes EncodeNetworkConfiguration(const NetworkConfiguration& configuration);

// What is wrong with the configuration, one sentence for each problem, by the rules of the network configuration module of IOS80.
// An empty list means that the module accepts the configuration. The module of IOS9 does not check the proxy settings:
// it takes a proxy with a server name that is not valid and a port of 0 (checksProxies is false).
std::vector<std::string> FindConfigurationProblems(const NetworkConfiguration& configuration, bool checksProxies = true);

}
