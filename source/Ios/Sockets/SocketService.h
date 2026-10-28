#pragma once

#include "Formats/Bytes.h"
#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Result.h"
#include "Ios/Sockets/SocketTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace Ios
{

// The numbers of the options that the System Menu asks the network interface for. The level of all of them is 0xFFFE.
constexpr std::uint32_t IpAddressOption = 0x1003;
constexpr std::uint32_t MacAddressOption = 0x1004;
constexpr std::uint32_t AddressCountOption = 0x4002;
constexpr std::uint32_t AddressTableOption = 0x4003;

// The socket stack (SO) of IOS, in place of /dev/net/ip/top.
//
// The stack of the console is a TCP/IP implementation. It works only after a program set it up with a network connection, and
// this console never has one: no access point is found and no cable is plugged in. So the stack stays down for ever, and the
// service answers what the module answers in that state, with the same numbers (docs/audit/Ios.md):
//
// * Every call that needs a socket fails with StackNotStarted. Nothing can open a socket, so there is none to use.
// * A program can start the network interface of the connection that the network configuration module chose. Then the
//   interface answers questions about itself: it has no address, and the address of its network card.
// * No host name can be resolved. The text forms of addresses are converted as always (ParseInternetAddress).
//
// All calls are thread safe.
class SocketService
{
public:
    // The stack uses the interface that the network configuration module has chosen. In the original the module tells the stack so.
    explicit SocketService(const NetworkConfigService& networkConfig);

    // Starts the network interface of the connection that the network configuration module chose. Always succeeds: when the module
    // chose no connection, nothing starts, and the questions about the interface fail. A second call changes nothing.
    // The SDK calls this only when the link status of the module is WiredInterfaceReady, LinkDown or LinkUp.
    Result<void> Startup();

    // The address of the console in the network, 0.0.0.0 while it is not connected.
    Ipv4Address GetHostId() const;

    // What the started interface says about itself. Only the options of the constants above are answered:
    // the IP address (4 bytes), the MAC address of the network card (6 bytes), the number of addresses of the stack (4 bytes, 2)
    // and the table of the addresses (empty: the stack has none).
    // StackNotStarted before Startup started an interface. InterfaceOptionNotSupported for any other option, or when the
    // capacity is smaller than the answer.
    Result<Formats::Bytes> GetInterfaceOption(std::uint32_t option, std::size_t capacity) const;

    // The calls that need a running stack. All fail with StackNotStarted.
    Result<void> Cleanup();
    Result<SocketId> CreateSocket(SocketType type, std::uint32_t protocol);
    Result<void> Bind(SocketId socket, const SocketAddress& address);
    Result<void> Connect(SocketId socket, const SocketAddress& address);
    Result<std::size_t> SendTo(SocketId socket, Formats::ByteView data, const SocketAddress* destination);
    Result<Formats::Bytes> ReceiveFrom(SocketId socket, std::size_t maximumLength);
    Result<std::size_t> Poll(std::span<PollEntry> entries, std::chrono::milliseconds timeout);
    Result<void> Shutdown(SocketId socket, ShutdownDirection direction);
    Result<void> Close(SocketId socket);

    // The addresses of a host name. The original asks the DNS servers of the connection. There are none, so no name is known:
    // HostNotFound for GetHostByName and NameLookupFailed for GetAddressInfo. An address in numbers needs no servers
    // and gives itself back from GetAddressInfo.
    Result<std::vector<Ipv4Address>> GetHostByName(std::string_view name) const;
    Result<std::vector<Ipv4Address>> GetAddressInfo(std::string_view host) const;

private:
    NetworkInterface GetStartedInterface() const;

    const NetworkConfigService& networkConfig;

    mutable std::mutex mutex;
    NetworkInterface startedInterface = NetworkInterface::None;
};

}
