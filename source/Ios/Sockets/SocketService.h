#pragma once

#include "Formats/Bytes.h"
#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Result.h"
#include "Ios/Sockets/SocketTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
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

// The stack has 24 sockets, numbered 0 to 23, and Poll looks at 24 of them at most.
constexpr std::size_t MaximumSocketCount = 24;

// The socket stack (SO) of IOS, in place of /dev/net/ip/top.
//
// The sockets are real sockets of Windows: the console is online through the PC (docs/DECISIONS.md, Online). A program sees the
// calls, the numbers and the errors of the original (docs/audit/Ios.md):
//
// * The stack is set up when a program starts it while the network configuration module has the link up, that is while the PC is
//   connected. Before that every call that needs a socket fails with StackNotStarted, as on a console without a network.
// * A program can start the network interface of the connection that the network configuration module chose. Then the
//   interface answers questions about itself: the address of the PC, and the address of its network card.
// * Host names are looked up by Windows. The text forms of addresses are converted as always (ParseInternetAddress).
//
// The calls block like the original's. All calls are thread safe: a socket that one thread closes ends the wait of another.
class SocketService
{
public:
    // The stack uses the interface that the network configuration module has chosen. In the original the module tells the stack so.
    explicit SocketService(const NetworkConfigService& networkConfig);

    // Closes every socket that is still open.
    ~SocketService();

    SocketService(const SocketService&) = delete;
    SocketService& operator=(const SocketService&) = delete;

    // Starts the network interface of the connection that the network configuration module chose, and sets the stack up when the
    // link is up. Always succeeds: when the module chose no connection, nothing starts, and the questions about the interface fail.
    // The SDK calls this only when the link status of the module is WiredInterfaceReady, LinkDown or LinkUp.
    Result<void> Startup();

    // The address of the console in the network: the PC's, or 0.0.0.0 while the stack is not set up.
    Ipv4Address GetHostId() const;

    // What the started interface says about itself. Only the options of the constants above are answered:
    // the IP address (4 bytes), the MAC address of the network card (6 bytes), the number of addresses of the stack (4 bytes, 2)
    // and the table of the addresses (empty).
    // StackNotStarted before Startup started an interface. InterfaceOptionNotSupported for any other option, or when the
    // capacity is smaller than the answer.
    Result<Formats::Bytes> GetInterfaceOption(std::uint32_t option, std::size_t capacity) const;

    // The calls that need a set up stack. Before that they all fail with StackNotStarted; then they fail with the error of the socket.
    // Cleanup closes every socket.
    Result<void> Cleanup();
    Result<SocketId> CreateSocket(SocketType type, std::uint32_t protocol);
    Result<void> Bind(SocketId socket, const SocketAddress& address);
    Result<void> Connect(SocketId socket, const SocketAddress& address);
    Result<std::size_t> SendTo(SocketId socket, Formats::ByteView data, const SocketAddress* destination);

    // Waits for data. An empty answer means that the other side closed the connection.
    Result<Formats::Bytes> ReceiveFrom(SocketId socket, std::size_t maximumLength);

    // Fills in the happened events of each entry and gives the number of entries that have one. A negative time-out waits for ever.
    // InvalidSocketArgument for more than MaximumSocketCount entries.
    Result<std::size_t> Poll(std::span<PollEntry> entries, std::chrono::milliseconds timeout);

    Result<void> Shutdown(SocketId socket, ShutdownDirection direction);
    Result<void> Close(SocketId socket);

    // The addresses of a host name. While the stack is not set up the console has no DNS server, so no name is known:
    // HostNotFound for GetHostByName and NameLookupFailed for GetAddressInfo. An address in numbers needs no servers
    // and gives itself back from GetAddressInfo.
    Result<std::vector<Ipv4Address>> GetHostByName(std::string_view name) const;
    Result<std::vector<Ipv4Address>> GetAddressInfo(std::string_view host) const;

private:
    // The socket of Windows behind a number. Windows numbers its sockets with pointer sized handles.
    using WindowsSocket = std::uintptr_t;

    Result<WindowsSocket> FindSocket(SocketId socket) const;
    Result<std::vector<Ipv4Address>> LookUp(std::string_view name) const;
    bool IsStackSetUp() const;
    void CloseAllSockets();

    const NetworkConfigService& networkConfig;

    mutable std::mutex mutex;
    NetworkInterface startedInterface = NetworkInterface::None;
    bool isStackSetUp = false;
    std::map<SocketId, WindowsSocket> openSockets;
};

}
