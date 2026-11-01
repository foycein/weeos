#include "Ios/Network/NetworkConfiguration.h"

#include "Formats/ByteReader.h"
#include "Ios/BigEndianBytes.h"

#include <algorithm>

namespace Ios
{

namespace
{

constexpr std::string_view DataDescription = "Network configuration";

// The header of the file.
constexpr std::size_t VersionOffset = 0;
constexpr std::size_t SelectedMediaOffset = 4;
constexpr std::size_t PermissionOffset = 5;
constexpr std::size_t FirstProfileOffset = 8;
constexpr std::size_t ProfileSize = 0x91C;

// Inside a connection. The offsets count from the start of the connection.
constexpr std::size_t FlagsOffset = 0x00;
constexpr std::size_t AddressOffset = 0x04;
constexpr std::size_t NetmaskOffset = 0x08;
constexpr std::size_t GatewayOffset = 0x0C;
constexpr std::size_t PrimaryDnsOffset = 0x10;
constexpr std::size_t SecondaryDnsOffset = 0x14;
constexpr std::size_t MtuOffset = 0x1A;
constexpr std::size_t TcpTimeoutOffset = 0x1C;
constexpr std::size_t HttpProxyOffset = 0x24;
constexpr std::size_t SecureProxyOffset = 0x16C;
constexpr std::size_t WirelessOffset = 0x7C0;

// The bits of the first byte of a connection.
constexpr Formats::Byte EnabledBit = 0x80;
constexpr Formats::Byte PathMtuRecoveryBit = 0x40;
constexpr Formats::Byte PassedConnectionTestBit = 0x20;
constexpr Formats::Byte ProxyBit = 0x10;
constexpr Formats::Byte PppoeBit = 0x08;
constexpr Formats::Byte DhcpForDnsBit = 0x04;
constexpr Formats::Byte DhcpForAddressBit = 0x02;
constexpr Formats::Byte WiredBit = 0x01;

// Inside a proxy. The offsets count from the start of the proxy.
constexpr std::size_t ProxyIsUsedOffset = 0;
constexpr std::size_t ProxyUsesCredentialsOffset = 1;
constexpr std::size_t ProxyServerNameOffset = 4;
constexpr std::size_t ProxyPortOffset = 0x104;
constexpr std::size_t ProxyUserNameOffset = 0x106;
constexpr std::size_t ProxyPasswordOffset = 0x127;

// Inside the wireless part. The offsets count from the start of the wireless part.
constexpr std::size_t RateSetOffset = 0x00;
constexpr std::size_t ConfigMethodOffset = 2;
constexpr std::size_t ByteAfterConfigMethodOffset = 3;
constexpr std::size_t SsidOffset = 4;
constexpr std::size_t SsidLengthOffset = 0x24;
constexpr std::size_t EncryptionOffset = 0x28;
constexpr std::size_t KeyLengthOffset = 0x2C;
constexpr std::size_t WordAfterKeyLengthOffset = 0x2E;
constexpr std::size_t KeyOffset = 0x30;
constexpr std::size_t ExtensionOffset = 0x70;

// Reading

bool HasBit(Formats::Byte flags, Formats::Byte bit)
{
    return (flags & bit) != 0;
}

ProxySettings DecodeProxy(Formats::ByteReader& reader, std::size_t proxyStart)
{
    ProxySettings proxy;

    reader.MoveTo(proxyStart + ProxyIsUsedOffset);
    proxy.isUsed = reader.ReadByte() == 1;
    reader.MoveTo(proxyStart + ProxyUsesCredentialsOffset);
    proxy.usesCredentials = reader.ReadByte() == 1;

    reader.MoveTo(proxyStart + ProxyServerNameOffset);
    proxy.serverName = reader.ReadText(MaximumProxyServerNameLength);
    reader.MoveTo(proxyStart + ProxyPortOffset);
    proxy.port = reader.ReadUint16();
    reader.MoveTo(proxyStart + ProxyUserNameOffset);
    proxy.userName = reader.ReadText(MaximumProxyUserNameLength);
    reader.MoveTo(proxyStart + ProxyPasswordOffset);
    proxy.password = reader.ReadText(MaximumProxyPasswordLength);
    return proxy;
}

WirelessSettings DecodeWireless(Formats::ByteReader& reader, std::size_t wirelessStart)
{
    WirelessSettings wireless;

    reader.MoveTo(wirelessStart + RateSetOffset);
    wireless.rateSet = reader.ReadUint16();
    reader.MoveTo(wirelessStart + ConfigMethodOffset);
    wireless.configMethod = reader.ReadByte();
    reader.MoveTo(wirelessStart + ByteAfterConfigMethodOffset);
    wireless.byteAfterConfigMethod = reader.ReadByte();

    reader.MoveTo(wirelessStart + SsidLengthOffset);
    std::size_t ssidLength = std::min<std::size_t>(reader.ReadUint16(), MaximumSsidLength);
    reader.MoveTo(wirelessStart + SsidOffset);
    Formats::ByteView ssidBytes = reader.ReadBytes(ssidLength);
    wireless.ssid.assign(ssidBytes.begin(), ssidBytes.end());

    reader.MoveTo(wirelessStart + EncryptionOffset);
    wireless.encryption = reader.ReadUint16();
    reader.MoveTo(wirelessStart + KeyLengthOffset);
    wireless.keyLengthOrWepKeyNumber = reader.ReadUint16();
    reader.MoveTo(wirelessStart + WordAfterKeyLengthOffset);
    wireless.wordAfterKeyLength = reader.ReadUint16();

    reader.MoveTo(wirelessStart + KeyOffset);
    wireless.key = reader.ReadByteArray<MaximumKeySize>();
    reader.MoveTo(wirelessStart + ExtensionOffset);
    wireless.extension = reader.ReadByteArray<WirelessExtensionSize>();
    return wireless;
}

ConnectionProfile DecodeProfile(Formats::ByteReader& reader, std::size_t profileStart)
{
    ConnectionProfile profile;

    reader.MoveTo(profileStart + FlagsOffset);
    Formats::Byte flags = reader.ReadByte();
    profile.isEnabled = HasBit(flags, EnabledBit);
    profile.usesPathMtuRecovery = HasBit(flags, PathMtuRecoveryBit);
    profile.hasPassedConnectionTest = HasBit(flags, PassedConnectionTestBit);
    profile.usesProxy = HasBit(flags, ProxyBit);
    profile.usesPppoe = HasBit(flags, PppoeBit);
    profile.usesDhcpForDns = HasBit(flags, DhcpForDnsBit);
    profile.usesDhcpForAddress = HasBit(flags, DhcpForAddressBit);
    profile.isWired = HasBit(flags, WiredBit);

    reader.MoveTo(profileStart + AddressOffset);
    profile.address = reader.ReadUint32();
    reader.MoveTo(profileStart + NetmaskOffset);
    profile.netmask = reader.ReadUint32();
    reader.MoveTo(profileStart + GatewayOffset);
    profile.gateway = reader.ReadUint32();
    reader.MoveTo(profileStart + PrimaryDnsOffset);
    profile.primaryDns = reader.ReadUint32();
    reader.MoveTo(profileStart + SecondaryDnsOffset);
    profile.secondaryDns = reader.ReadUint32();

    reader.MoveTo(profileStart + MtuOffset);
    profile.mtu = reader.ReadUint16();
    reader.MoveTo(profileStart + TcpTimeoutOffset);
    profile.tcpTimeout = reader.ReadUint32();

    profile.httpProxy = DecodeProxy(reader, profileStart + HttpProxyOffset);
    profile.secureProxy = DecodeProxy(reader, profileStart + SecureProxyOffset);
    profile.wireless = DecodeWireless(reader, profileStart + WirelessOffset);
    return profile;
}

// Writing

void EncodeProxy(Formats::Bytes& bytes, std::size_t proxyStart, const ProxySettings& proxy)
{
    PutByte(bytes, proxyStart + ProxyIsUsedOffset, proxy.isUsed ? 1 : 0);
    PutByte(bytes, proxyStart + ProxyUsesCredentialsOffset, proxy.usesCredentials ? 1 : 0);
    PutText(bytes, proxyStart + ProxyServerNameOffset, proxy.serverName, MaximumProxyServerNameLength);
    PutUint16(bytes, proxyStart + ProxyPortOffset, proxy.port);
    PutText(bytes, proxyStart + ProxyUserNameOffset, proxy.userName, MaximumProxyUserNameLength);
    PutText(bytes, proxyStart + ProxyPasswordOffset, proxy.password, MaximumProxyPasswordLength);
}

void EncodeWireless(Formats::Bytes& bytes, std::size_t wirelessStart, const WirelessSettings& wireless)
{
    PutUint16(bytes, wirelessStart + RateSetOffset, wireless.rateSet);
    PutByte(bytes, wirelessStart + ConfigMethodOffset, wireless.configMethod);
    PutByte(bytes, wirelessStart + ByteAfterConfigMethodOffset, wireless.byteAfterConfigMethod);

    PutText(bytes, wirelessStart + SsidOffset, wireless.ssid, MaximumSsidLength);
    PutUint16(bytes, wirelessStart + SsidLengthOffset, static_cast<std::uint16_t>(wireless.ssid.size()));

    PutUint16(bytes, wirelessStart + EncryptionOffset, wireless.encryption);
    PutUint16(bytes, wirelessStart + KeyLengthOffset, wireless.keyLengthOrWepKeyNumber);
    PutUint16(bytes, wirelessStart + WordAfterKeyLengthOffset, wireless.wordAfterKeyLength);
    PutBytes(bytes, wirelessStart + KeyOffset, wireless.key);
    PutBytes(bytes, wirelessStart + ExtensionOffset, wireless.extension);
}

Formats::Byte FlagsOf(const ConnectionProfile& profile)
{
    Formats::Byte flags = 0;
    if (profile.isEnabled)
        flags |= EnabledBit;
    if (profile.usesPathMtuRecovery)
        flags |= PathMtuRecoveryBit;
    if (profile.hasPassedConnectionTest)
        flags |= PassedConnectionTestBit;
    if (profile.usesProxy)
        flags |= ProxyBit;
    if (profile.usesPppoe)
        flags |= PppoeBit;
    if (profile.usesDhcpForDns)
        flags |= DhcpForDnsBit;
    if (profile.usesDhcpForAddress)
        flags |= DhcpForAddressBit;
    if (profile.isWired)
        flags |= WiredBit;
    return flags;
}

void EncodeProfile(Formats::Bytes& bytes, std::size_t profileStart, const ConnectionProfile& profile)
{
    PutByte(bytes, profileStart + FlagsOffset, FlagsOf(profile));
    PutUint32(bytes, profileStart + AddressOffset, profile.address);
    PutUint32(bytes, profileStart + NetmaskOffset, profile.netmask);
    PutUint32(bytes, profileStart + GatewayOffset, profile.gateway);
    PutUint32(bytes, profileStart + PrimaryDnsOffset, profile.primaryDns);
    PutUint32(bytes, profileStart + SecondaryDnsOffset, profile.secondaryDns);
    PutUint16(bytes, profileStart + MtuOffset, profile.mtu);
    PutUint32(bytes, profileStart + TcpTimeoutOffset, profile.tcpTimeout);

    EncodeProxy(bytes, profileStart + HttpProxyOffset, profile.httpProxy);
    EncodeProxy(bytes, profileStart + SecureProxyOffset, profile.secureProxy);
    EncodeWireless(bytes, profileStart + WirelessOffset, profile.wireless);
}

}

Result<NetworkConfiguration> DecodeNetworkConfiguration(Formats::ByteView fileBytes)
{
    if (fileBytes.size() != NetworkConfigurationFileSize)
        return IosError::StorageFailure;

    Formats::ByteReader reader(fileBytes, DataDescription);
    NetworkConfiguration configuration;
    configuration.version = reader.ReadUint32();
    configuration.selectedMedia = static_cast<SelectedMedia>(reader.ReadByte());
    configuration.wiiConnect24Permission = reader.ReadByte();

    for (std::size_t profileNumber = 0; profileNumber < ConnectionProfileCount; ++profileNumber)
    {
        std::size_t profileStart = FirstProfileOffset + profileNumber * ProfileSize;
        configuration.profiles[profileNumber] = DecodeProfile(reader, profileStart);
    }
    return configuration;
}

Formats::Bytes EncodeNetworkConfiguration(const NetworkConfiguration& configuration)
{
    Formats::Bytes bytes(NetworkConfigurationFileSize);
    PutUint32(bytes, VersionOffset, configuration.version);
    PutByte(bytes, SelectedMediaOffset, static_cast<Formats::Byte>(configuration.selectedMedia));
    PutByte(bytes, PermissionOffset, configuration.wiiConnect24Permission);

    for (std::size_t profileNumber = 0; profileNumber < ConnectionProfileCount; ++profileNumber)
    {
        std::size_t profileStart = FirstProfileOffset + profileNumber * ProfileSize;
        EncodeProfile(bytes, profileStart, configuration.profiles[profileNumber]);
    }
    return bytes;
}

}
