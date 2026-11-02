#include "Ios/Sockets/SocketService.h"

#include "Ios/BigEndianBytes.h"
#include "Ios/Network/PcConnection.h"
#include "Ios/Sockets/InternetAddress.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <optional>
#include <string>

namespace Ios
{

namespace
{

// The address of the adapter of a wired connection is not known here, so it is all zero.
constexpr MacAddress UnknownAdapterAddress = {};

// The stack of the console keeps two addresses: the loopback address and the address of the interface.
constexpr std::uint32_t AddressCount = 2;

constexpr WORD WinsockVersion = MAKEWORD(2, 2);

// A poll of Windows that waits for ever.
constexpr int WaitForEver = -1;

Formats::Bytes MakeBytesOfNumber(std::uint32_t number)
{
    Formats::Bytes bytes(sizeof(number));
    PutUint32(bytes, 0, number);
    return bytes;
}

// The MAC address of the network card of the interface. The wireless card has the address of the console.
MacAddress MacAddressOf(NetworkInterface interface, const NetworkConfigService& networkConfig)
{
    if (interface == NetworkInterface::Wireless)
        return networkConfig.GetWirelessMacAddress();

    return UnknownAdapterAddress;
}

// What the interface answers to the option. Nothing when the option is not one of those that are rebuilt.
std::optional<Formats::Bytes> AnswerOf(std::uint32_t option, NetworkInterface interface, Ipv4Address hostId,
    const NetworkConfigService& networkConfig)
{
    if (option == IpAddressOption)
        return MakeBytesOfNumber(hostId);

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

// The error of the console's stack for the error that Windows gives a socket call. The console numbers its errors in the
// alphabetical order of the POSIX names (IosError.h).
IosError ErrorOfWindows(int windowsError)
{
    switch (windowsError)
    {
    case WSAEADDRINUSE:
        return IosError::AddressInUse;
    case WSAEAFNOSUPPORT:
        return IosError::AddressFamilyNotSupported;
    case WSAENOTSOCK:
        return IosError::BadSocket;
    case WSAECONNABORTED:
        return IosError::ConnectionAborted;
    case WSAECONNREFUSED:
        return IosError::ConnectionRefused;
    case WSAECONNRESET:
        return IosError::ConnectionReset;
    case WSAEHOSTUNREACH:
        return IosError::HostUnreachable;
    case WSAEINVAL:
    case WSAEFAULT:
        return IosError::InvalidSocketArgument;
    case WSAEISCONN:
        return IosError::AlreadyConnected;
    case WSAEMFILE:
        return IosError::TooManySockets;
    case WSAEMSGSIZE:
        return IosError::MessageTooLong;
    case WSAENETDOWN:
        return IosError::NetworkDown;
    case WSAENETUNREACH:
        return IosError::NetworkUnreachable;
    case WSAENOTCONN:
        return IosError::NotConnected;
    case WSAEPROTONOSUPPORT:
        return IosError::ProtocolNotSupported;
    case WSAEPROTOTYPE:
        return IosError::WrongProtocolType;
    case WSAETIMEDOUT:
        return IosError::TimedOut;
    default:
        return IosError::SocketFailed;
    }
}

IosError LastWindowsError()
{
    return ErrorOfWindows(WSAGetLastError());
}

sockaddr_in WindowsAddressOf(const SocketAddress& address)
{
    sockaddr_in windowsAddress = {};
    windowsAddress.sin_family = AF_INET;
    windowsAddress.sin_addr.s_addr = htonl(address.address);
    windowsAddress.sin_port = htons(address.port);
    return windowsAddress;
}

int WindowsTypeOf(SocketType type)
{
    if (type == SocketType::Datagram)
        return SOCK_DGRAM;

    return SOCK_STREAM;
}

int WindowsDirectionOf(ShutdownDirection direction)
{
    if (direction == ShutdownDirection::Receive)
        return SD_RECEIVE;

    if (direction == ShutdownDirection::Send)
        return SD_SEND;

    return SD_BOTH;
}

// What a program waits for, in the events of Windows. Windows always reports errors and a closed connection.
SHORT WindowsEventsOf(std::uint16_t requestedEvents)
{
    SHORT events = 0;
    if ((requestedEvents & (PollReadable | PollReadableUrgent)) != 0)
        events |= POLLRDNORM;

    if ((requestedEvents & PollWritable) != 0)
        events |= POLLWRNORM;

    return events;
}

// What happened, in the events of the console. A closed connection can be read: the read gives its end.
std::uint16_t ConsoleEventsOf(SHORT happenedEvents)
{
    std::uint16_t events = 0;
    if ((happenedEvents & (POLLRDNORM | POLLHUP)) != 0)
        events |= PollReadable;

    if ((happenedEvents & POLLWRNORM) != 0)
        events |= PollWritable;

    if ((happenedEvents & POLLERR) != 0)
        events |= PollError;

    if ((happenedEvents & POLLHUP) != 0)
        events |= PollHungUp;

    if ((happenedEvents & POLLNVAL) != 0)
        events |= PollInvalid;

    return events;
}

int WindowsTimeOutOf(std::chrono::milliseconds timeout)
{
    if (timeout.count() < 0)
        return WaitForEver;

    return static_cast<int>(timeout.count());
}

}

SocketService::SocketService(const NetworkConfigService& networkConfigToUse)
    : networkConfig(networkConfigToUse)
{
    WSADATA winsockInformation;
    WSAStartup(WinsockVersion, &winsockInformation);
}

SocketService::~SocketService()
{
    CloseAllSockets();
    WSACleanup();
}

Result<void> SocketService::Startup()
{
    NetworkInterface chosenInterface = networkConfig.GetChosenInterface();
    bool isLinkUp = networkConfig.GetLinkStatus() == LinkStatus::LinkUp;

    std::lock_guard<std::mutex> lock(mutex);
    if (startedInterface == NetworkInterface::None)
        startedInterface = chosenInterface;

    if (isLinkUp)
        isStackSetUp = true;

    return Result<void>();
}

bool SocketService::IsStackSetUp() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return isStackSetUp;
}

Ipv4Address SocketService::GetHostId() const
{
    if (!IsStackSetUp())
        return NoIpv4Address;

    return FindPcAddress();
}

Result<Formats::Bytes> SocketService::GetInterfaceOption(std::uint32_t option, std::size_t capacity) const
{
    NetworkInterface interface = NetworkInterface::None;
    {
        std::lock_guard<std::mutex> lock(mutex);
        interface = startedInterface;
    }
    if (interface == NetworkInterface::None)
        return IosError::StackNotStarted;

    std::optional<Formats::Bytes> answer = AnswerOf(option, interface, GetHostId(), networkConfig);
    if (!answer || answer->size() > capacity)
        return IosError::InterfaceOptionNotSupported;

    return *answer;
}

Result<SocketService::WindowsSocket> SocketService::FindSocket(SocketId socket) const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!isStackSetUp)
        return IosError::StackNotStarted;

    auto found = openSockets.find(socket);
    if (found == openSockets.end())
        return IosError::BadSocket;

    return found->second;
}

void SocketService::CloseAllSockets()
{
    std::map<SocketId, WindowsSocket> socketsToClose;
    {
        std::lock_guard<std::mutex> lock(mutex);
        socketsToClose.swap(openSockets);
    }

    for (const auto& [socket, windowsSocket] : socketsToClose)
        closesocket(windowsSocket);
}

Result<void> SocketService::Cleanup()
{
    if (!IsStackSetUp())
        return IosError::StackNotStarted;

    CloseAllSockets();
    return Result<void>();
}

// The lowest free number, as the console gives them.
Result<SocketId> SocketService::CreateSocket(SocketType type, std::uint32_t protocol)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!isStackSetUp)
        return IosError::StackNotStarted;

    SocketId freeNumber = 0;
    while (openSockets.contains(freeNumber))
        ++freeNumber;

    if (freeNumber >= static_cast<SocketId>(MaximumSocketCount))
        return IosError::TooManySockets;

    SOCKET windowsSocket = socket(AF_INET, WindowsTypeOf(type), static_cast<int>(protocol));
    if (windowsSocket == INVALID_SOCKET)
        return LastWindowsError();

    openSockets[freeNumber] = windowsSocket;
    return freeNumber;
}

Result<void> SocketService::Bind(SocketId socket, const SocketAddress& address)
{
    Result<WindowsSocket> windowsSocket = FindSocket(socket);
    if (windowsSocket.Failed())
        return windowsSocket.Error();

    sockaddr_in windowsAddress = WindowsAddressOf(address);
    if (bind(windowsSocket.Value(), reinterpret_cast<const sockaddr*>(&windowsAddress), sizeof(windowsAddress)) != 0)
        return LastWindowsError();

    return Result<void>();
}

Result<void> SocketService::Connect(SocketId socket, const SocketAddress& address)
{
    Result<WindowsSocket> windowsSocket = FindSocket(socket);
    if (windowsSocket.Failed())
        return windowsSocket.Error();

    sockaddr_in windowsAddress = WindowsAddressOf(address);
    if (connect(windowsSocket.Value(), reinterpret_cast<const sockaddr*>(&windowsAddress), sizeof(windowsAddress)) != 0)
        return LastWindowsError();

    return Result<void>();
}

Result<std::size_t> SocketService::SendTo(SocketId socket, Formats::ByteView data, const SocketAddress* destination)
{
    Result<WindowsSocket> windowsSocket = FindSocket(socket);
    if (windowsSocket.Failed())
        return windowsSocket.Error();

    const char* bytes = reinterpret_cast<const char*>(data.data());
    int length = static_cast<int>(data.size());
    int sent = 0;
    if (destination == nullptr)
    {
        sent = send(windowsSocket.Value(), bytes, length, 0);
    }
    else
    {
        sockaddr_in windowsAddress = WindowsAddressOf(*destination);
        sent = sendto(windowsSocket.Value(), bytes, length, 0, reinterpret_cast<const sockaddr*>(&windowsAddress), sizeof(windowsAddress));
    }

    if (sent == SOCKET_ERROR)
        return LastWindowsError();

    return static_cast<std::size_t>(sent);
}

Result<Formats::Bytes> SocketService::ReceiveFrom(SocketId socket, std::size_t maximumLength)
{
    Result<WindowsSocket> windowsSocket = FindSocket(socket);
    if (windowsSocket.Failed())
        return windowsSocket.Error();

    Formats::Bytes received(maximumLength);
    int length = recv(windowsSocket.Value(), reinterpret_cast<char*>(received.data()), static_cast<int>(maximumLength), 0);
    if (length == SOCKET_ERROR)
        return LastWindowsError();

    received.resize(static_cast<std::size_t>(length));
    return received;
}

// An entry whose socket is not open gets no events, as in the original.
Result<std::size_t> SocketService::Poll(std::span<PollEntry> entries, std::chrono::milliseconds timeout)
{
    if (!IsStackSetUp())
        return IosError::StackNotStarted;

    if (entries.size() > MaximumSocketCount)
        return IosError::InvalidSocketArgument;

    std::vector<WSAPOLLFD> windowsEntries;
    std::vector<PollEntry*> entriesOfWindows;
    for (PollEntry& entry : entries)
    {
        entry.happenedEvents = 0;
        Result<WindowsSocket> windowsSocket = FindSocket(entry.socket);
        if (windowsSocket.Failed())
            continue;

        windowsEntries.push_back({windowsSocket.Value(), WindowsEventsOf(entry.requestedEvents), 0});
        entriesOfWindows.push_back(&entry);
    }

    if (windowsEntries.empty())
        return std::size_t{0};

    int ready = WSAPoll(windowsEntries.data(), static_cast<ULONG>(windowsEntries.size()), WindowsTimeOutOf(timeout));
    if (ready == SOCKET_ERROR)
        return LastWindowsError();

    std::size_t entriesWithEvents = 0;
    for (std::size_t index = 0; index < windowsEntries.size(); ++index)
    {
        std::uint16_t events = ConsoleEventsOf(windowsEntries[index].revents);
        entriesOfWindows[index]->happenedEvents = events;
        if (events != 0)
            ++entriesWithEvents;
    }
    return entriesWithEvents;
}

Result<void> SocketService::Shutdown(SocketId socket, ShutdownDirection direction)
{
    Result<WindowsSocket> windowsSocket = FindSocket(socket);
    if (windowsSocket.Failed())
        return windowsSocket.Error();

    if (shutdown(windowsSocket.Value(), WindowsDirectionOf(direction)) != 0)
        return LastWindowsError();

    return Result<void>();
}

Result<void> SocketService::Close(SocketId socket)
{
    WindowsSocket windowsSocket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!isStackSetUp)
            return IosError::StackNotStarted;

        auto found = openSockets.find(socket);
        if (found == openSockets.end())
            return IosError::BadSocket;

        windowsSocket = found->second;
        openSockets.erase(found);
    }

    closesocket(windowsSocket);
    return Result<void>();
}

Result<std::vector<Ipv4Address>> SocketService::LookUp(std::string_view name) const
{
    addrinfo hints = {};
    hints.ai_family = AF_INET;

    addrinfo* found = nullptr;
    if (getaddrinfo(std::string(name).c_str(), nullptr, &hints, &found) != 0)
        return IosError::NameLookupFailed;

    std::vector<Ipv4Address> addresses;
    for (addrinfo* entry = found; entry != nullptr; entry = entry->ai_next)
    {
        const auto* address = reinterpret_cast<const sockaddr_in*>(entry->ai_addr);
        addresses.push_back(ntohl(address->sin_addr.s_addr));
    }
    freeaddrinfo(found);
    return addresses;
}

Result<std::vector<Ipv4Address>> SocketService::GetHostByName(std::string_view name) const
{
    if (!IsStackSetUp())
        return IosError::HostNotFound;

    Result<std::vector<Ipv4Address>> addresses = LookUp(name);
    if (addresses.Failed())
        return IosError::HostNotFound;

    return addresses;
}

Result<std::vector<Ipv4Address>> SocketService::GetAddressInfo(std::string_view host) const
{
    std::optional<Ipv4Address> address = ParseInternetAddress(host);
    if (address)
        return std::vector<Ipv4Address>{*address};

    if (!IsStackSetUp())
        return IosError::NameLookupFailed;

    return LookUp(host);
}

}
