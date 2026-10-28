#include "Ios/Sockets/SocketService.h"

#include "Ios/BigEndianBytes.h"
#include "Ios/Sockets/InternetAddress.h"

namespace Ios
{

namespace
{

constexpr Ipv4Address NoAddress = 0;

// The stack of the console keeps two addresses: the loopback address and the address of the interface.
constexpr std::uint32_t AddressCount = 2;

Formats::Bytes MakeBytesOfNumber(std::uint32_t number)
{
    Formats::Bytes bytes(sizeof(number));
    PutUint32(bytes, 0, number);
    return bytes;
}

// The MAC address of the network card of the interface. The wireless card has the address of the console. The adapter of a
// wired connection is not known here, so it has the address zero.
MacAddress MacAddressOf(NetworkInterface interface, const NetworkConfigService& networkConfig)
{
    if (interface == NetworkInterface::Wireless)
        return networkConfig.GetWirelessMacAddress();

    return MacAddress{};
}

// What the interface answers to the option. Nothing when the option is not one of those that are rebuilt.
std::optional<Formats::Bytes> AnswerOf(std::uint32_t option, NetworkInterface interface, const NetworkConfigService& networkConfig)
{
    if (option == IpAddressOption)
        return MakeBytesOfNumber(NoAddress);

    if (option == MacAddressOption)
    {
        MacAddress macAddress = MacAddressOf(interface, networkConfig);
        return Formats::Bytes(macAddress.begin(), macAddress.end());
    }

    if (option == AddressCountOption)
        return MakeBytesOfNumber(AddressCount);

    if (option == AddressTableOption)
        return Formats::Bytes();

    return std::nullopt;
}

}

SocketService::SocketService(const NetworkConfigService& networkConfigToUse)
    : networkConfig(networkConfigToUse)
{
}

Result<void> SocketService::Startup()
{
    NetworkInterface chosenInterface = networkConfig.GetChosenInterface();

    std::lock_guard<std::mutex> lock(mutex);
    if (startedInterface == NetworkInterface::None)
        startedInterface = chosenInterface;

    return Result<void>();
}

Ipv4Address SocketService::GetHostId() const
{
    return NoAddress;
}

NetworkInterface SocketService::GetStartedInterface() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return startedInterface;
}

Result<Formats::Bytes> SocketService::GetInterfaceOption(std::uint32_t option, std::size_t capacity) const
{
    NetworkInterface interface = GetStartedInterface();
    if (interface == NetworkInterface::None)
        return IosError::StackNotStarted;

    std::optional<Formats::Bytes> answer = AnswerOf(option, interface, networkConfig);
    if (!answer || answer->size() > capacity)
        return IosError::InterfaceOptionNotSupported;

    return *answer;
}

Result<void> SocketService::Cleanup()
{
    return IosError::StackNotStarted;
}

Result<SocketId> SocketService::CreateSocket(SocketType, std::uint32_t)
{
    return IosError::StackNotStarted;
}

Result<void> SocketService::Bind(SocketId, const SocketAddress&)
{
    return IosError::StackNotStarted;
}

Result<void> SocketService::Connect(SocketId, const SocketAddress&)
{
    return IosError::StackNotStarted;
}

Result<std::size_t> SocketService::SendTo(SocketId, Formats::ByteView, const SocketAddress*)
{
    return IosError::StackNotStarted;
}

Result<Formats::Bytes> SocketService::ReceiveFrom(SocketId, std::size_t)
{
    return IosError::StackNotStarted;
}

Result<std::size_t> SocketService::Poll(std::span<PollEntry>, std::chrono::milliseconds)
{
    return IosError::StackNotStarted;
}

Result<void> SocketService::Shutdown(SocketId, ShutdownDirection)
{
    return IosError::StackNotStarted;
}

Result<void> SocketService::Close(SocketId)
{
    return IosError::StackNotStarted;
}

Result<std::vector<Ipv4Address>> SocketService::GetHostByName(std::string_view) const
{
    return IosError::HostNotFound;
}

Result<std::vector<Ipv4Address>> SocketService::GetAddressInfo(std::string_view host) const
{
    std::optional<Ipv4Address> address = ParseInternetAddress(host);
    if (!address)
        return IosError::NameLookupFailed;

    return std::vector<Ipv4Address>{*address};
}

}
