#include "Ios/Network/NetworkConfiguration.h"

#include <bit>
#include <format>
#include <optional>
#include <string_view>

namespace Ios
{

namespace
{

// The rules are the ones of the network configuration module of IOS80 (docs/audit/Ios.md).

constexpr std::uint32_t SupportedVersion = 0;
constexpr std::uint8_t UsedPermissionBits = 0x07;
constexpr std::uint16_t UnusedRateSetBits = 0xF000;
constexpr std::uint16_t DefaultMtu = 0;
constexpr std::uint16_t LowestMtu = 68;
constexpr std::uint16_t HighestMtu = 1500;
constexpr std::uint16_t WepKeyCount = 4;
constexpr std::size_t LowestPassphraseLength = 8;
constexpr std::size_t HighestPassphraseLength = 64;
constexpr int BitsPerIpv4Address = 32;
constexpr Ipv4Address SingleAddressNetmask = 0xFFFFFFFF;
constexpr Ipv4Address PointToPointNetmask = 0xFFFFFFFE;

using Problems = std::vector<std::string>;

void AddProblem(Problems& problems, int connectionNumber, std::string_view text)
{
    problems.push_back(std::format("connection {}: {}", connectionNumber, text));
}

bool IsMedia(SelectedMedia media)
{
    return media == SelectedMedia::None || media == SelectedMedia::Wireless || media == SelectedMedia::Wired;
}

// Addresses

// A netmask is a row of one bits followed by a row of zero bits: the ones at the top and the zeros at the bottom fill all its bits.
bool IsNetmask(Ipv4Address netmask)
{
    int onesAtTheTop = std::countl_one(netmask);
    int zerosAtTheBottom = std::countr_zero(netmask);
    return onesAtTheTop + zerosAtTheBottom == BitsPerIpv4Address;
}

// The address of the network itself and the broadcast address of the network cannot belong to a computer.
// A network with two or one address (netmask 255.255.255.254 and 255.255.255.255) has neither.
bool IsAddressOfTheNetworkOrBroadcast(Ipv4Address netmask, Ipv4Address address)
{
    if (netmask == SingleAddressNetmask || netmask == PointToPointNetmask)
        return false;

    Ipv4Address hostPart = address & ~netmask;
    return hostPart == 0 || hostPart == ~netmask;
}

// What is wrong with an address, a netmask and a gateway that were typed in. Nothing is wrong with a gateway of 0.0.0.0: there is none.
std::optional<std::string_view> FindAddressProblem(Ipv4Address address, Ipv4Address netmask, Ipv4Address gateway)
{
    if (!IsNetmask(netmask))
        return "the netmask is not a netmask";

    if (address == 0)
        return "the address is 0.0.0.0";

    if (IsAddressOfTheNetworkOrBroadcast(netmask, address))
        return "the address is the address of the network or its broadcast address";

    if (gateway == 0)
        return std::nullopt;

    if (IsAddressOfTheNetworkOrBroadcast(netmask, gateway))
        return "the gateway is the address of the network or its broadcast address";

    bool isInTheSameNetwork = (address & netmask) == (gateway & netmask);
    if (netmask != SingleAddressNetmask && !isInTheSameNetwork)
        return "the gateway is not in the network of the address";

    if (address == gateway)
        return "the gateway is the address itself";

    return std::nullopt;
}

// Names of servers

bool IsServerNameCharacter(char character)
{
    bool isDigit = character >= '0' && character <= '9';
    bool isLetter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    return isDigit || isLetter || character == '-' || character == '_' || character == '.';
}

// Letters, digits, minus, underscore and dots between the parts. A name does not start with a dot and has no two dots in a row.
bool IsServerName(const std::string& name)
{
    if (name.empty() || name.size() > MaximumProxyServerNameLength || name.front() == '.')
        return false;

    if (name.find("..") != std::string::npos)
        return false;

    for (char character : name)
    {
        if (!IsServerNameCharacter(character))
            return false;
    }
    return true;
}

// Checks

void CheckTextLength(const std::string& text, std::size_t maximumLength, std::string_view what, int connectionNumber, Problems& problems)
{
    if (text.size() <= maximumLength)
        return;

    AddProblem(problems, connectionNumber, std::format("{} is longer than {} characters", what, maximumLength));
}

void CheckTextsFitTheirFields(const ConnectionProfile& profile, int connectionNumber, Problems& problems)
{
    for (const ProxySettings* proxy : {&profile.httpProxy, &profile.secureProxy})
    {
        CheckTextLength(proxy->serverName, MaximumProxyServerNameLength, "a proxy server name", connectionNumber, problems);
        CheckTextLength(proxy->userName, MaximumProxyUserNameLength, "a proxy user name", connectionNumber, problems);
        CheckTextLength(proxy->password, MaximumProxyPasswordLength, "a proxy password", connectionNumber, problems);
    }

    CheckTextLength(profile.wireless.ssid, MaximumSsidLength, "the name of the access point", connectionNumber, problems);
}

void CheckProxy(const ProxySettings& proxy, std::string_view label, int connectionNumber, Problems& problems)
{
    if (!proxy.isUsed)
        return;

    if (!IsServerName(proxy.serverName))
        AddProblem(problems, connectionNumber, std::format("the {} server name is not valid", label));

    if (proxy.port == 0)
        AddProblem(problems, connectionNumber, std::format("the {} port is 0", label));
}

bool IsWepEncryption(std::uint16_t encryption)
{
    return encryption == EncryptionWep64 || encryption == EncryptionWep128;
}

bool UsesPassphrase(std::uint16_t encryption)
{
    return encryption == EncryptionWpaTkip || encryption == EncryptionWpa2Aes || encryption == EncryptionWpaAes;
}

void CheckEncryption(const WirelessSettings& wireless, int connectionNumber, Problems& problems)
{
    std::uint16_t keyNumberOrLength = wireless.keyLengthOrWepKeyNumber;

    if (IsWepEncryption(wireless.encryption) && keyNumberOrLength >= WepKeyCount)
    {
        std::string problem = std::format("the WEP key number {} is not 0 to {}", keyNumberOrLength, WepKeyCount - 1);
        AddProblem(problems, connectionNumber, problem);
    }

    bool lengthIsAllowed = keyNumberOrLength >= LowestPassphraseLength && keyNumberOrLength <= HighestPassphraseLength;
    if (UsesPassphrase(wireless.encryption) && !lengthIsAllowed)
    {
        std::string problem = std::format("the length {} of the passphrase is not {} to {}", keyNumberOrLength, LowestPassphraseLength,
            HighestPassphraseLength);
        AddProblem(problems, connectionNumber, problem);
    }
}

// The connections that are set up with several securities are kept as they are and not looked at.
void CheckWireless(const WirelessSettings& wireless, int connectionNumber, Problems& problems)
{
    if ((wireless.rateSet & UnusedRateSetBits) != 0)
        AddProblem(problems, connectionNumber, std::format("the rate set {:#x} uses bits that are not defined", wireless.rateSet));

    if (wireless.configMethod == ConfigMethodManual || wireless.configMethod == ConfigMethodSearch)
        CheckEncryption(wireless, connectionNumber, problems);
    else if (wireless.configMethod != ConfigMethodWifiUsbConnector && wireless.configMethod != ConfigMethodSeveralSecurities)
        AddProblem(problems, connectionNumber, std::format("the config method {} is not known", wireless.configMethod));
}

void CheckEnabledConnection(const ConnectionProfile& profile, int connectionNumber, bool checksProxies, Problems& problems)
{
    if (profile.usesPppoe)
        AddProblem(problems, connectionNumber, "PPPoE is not supported");

    if (!profile.usesDhcpForAddress)
    {
        std::optional<std::string_view> addressProblem = FindAddressProblem(profile.address, profile.netmask, profile.gateway);
        if (addressProblem)
            AddProblem(problems, connectionNumber, *addressProblem);
    }

    bool usesDefaultMtu = profile.mtu == DefaultMtu;
    bool isMtuInRange = profile.mtu >= LowestMtu && profile.mtu <= HighestMtu;
    if (!usesDefaultMtu && !isMtuInRange)
        AddProblem(problems, connectionNumber, std::format("the MTU {} is not 0 or {} to {}", profile.mtu, LowestMtu, HighestMtu));

    if (checksProxies && profile.usesProxy)
    {
        CheckProxy(profile.httpProxy, "http proxy", connectionNumber, problems);
        CheckProxy(profile.secureProxy, "ssl proxy", connectionNumber, problems);
    }
}

}

bool IsUsableWirelessConnection(const ConnectionProfile& profile)
{
    return profile.isEnabled && !profile.isWired;
}

bool IsUsableWiredConnection(const ConnectionProfile& profile)
{
    return profile.isEnabled && profile.isWired;
}

std::vector<std::string> FindConfigurationProblems(const NetworkConfiguration& configuration, bool checksProxies)
{
    Problems problems;

    if (configuration.version != SupportedVersion)
        problems.push_back(std::format("the version {} is not 0", configuration.version));

    if (!IsMedia(configuration.selectedMedia))
        problems.push_back(std::format("the selected media {} is not 0, 1 or 2", static_cast<int>(configuration.selectedMedia)));

    bool usesUnknownPermissionBits = (configuration.wiiConnect24Permission & ~UsedPermissionBits) != 0;
    if (usesUnknownPermissionBits)
    {
        problems.push_back(std::format("the WiiConnect24 permission {:#x} uses more than the three lowest bits",
            configuration.wiiConnect24Permission));
    }

    bool hasUsableWirelessConnection = false;
    bool hasUsableWiredConnection = false;
    for (std::size_t profileNumber = 0; profileNumber < ConnectionProfileCount; ++profileNumber)
    {
        const ConnectionProfile& profile = configuration.profiles[profileNumber];
        int connectionNumber = static_cast<int>(profileNumber) + 1;

        CheckTextsFitTheirFields(profile, connectionNumber, problems);
        if (profile.isEnabled)
            CheckEnabledConnection(profile, connectionNumber, checksProxies, problems);

        if (IsUsableWirelessConnection(profile))
        {
            hasUsableWirelessConnection = true;
            if (configuration.selectedMedia == SelectedMedia::Wireless)
                CheckWireless(profile.wireless, connectionNumber, problems);
        }

        if (IsUsableWiredConnection(profile))
            hasUsableWiredConnection = true;
    }

    if (configuration.selectedMedia == SelectedMedia::Wireless && !hasUsableWirelessConnection)
        problems.push_back("wireless is selected but no wireless connection is enabled");

    if (configuration.selectedMedia == SelectedMedia::Wired && !hasUsableWiredConnection)
        problems.push_back("wired is selected but no wired connection is enabled");

    return problems;
}

}
