#include "SystemMenu/OwnServer.h"

#include <charconv>
#include <chrono>
#include <format>
#include <limits>
#include <vector>

namespace SystemMenu
{

namespace
{

// A reply whose header is longer than this is not read further.
constexpr std::size_t MaximumHeaderSize = 16 * 1024;
constexpr std::size_t ReceiveChunkSize = 1024;

constexpr std::string_view HeaderEnd = "\r\n\r\n";
constexpr std::string_view LineEnd = "\r\n";
constexpr std::string_view HttpVersionStart = "HTTP/";
constexpr std::size_t StatusCodeLength = 3;

// The port after the last colon, or nothing when there is none or it is not a port.
std::optional<std::uint16_t> PortOf(std::string_view text)
{
    unsigned int port = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    bool isWholeNumber = error == std::errc() && end == text.data() + text.size();
    if (!isWholeNumber || port == 0 || port > std::numeric_limits<std::uint16_t>::max())
        return std::nullopt;

    return static_cast<std::uint16_t>(port);
}

// "HTTP/1.1 200 OK" gives 200; anything else 0.
int StatusCodeOf(std::string_view statusLine)
{
    if (!statusLine.starts_with(HttpVersionStart))
        return 0;

    std::size_t space = statusLine.find(' ');
    if (space == std::string_view::npos)
        return 0;

    std::string_view digits = statusLine.substr(space + 1, StatusCodeLength);
    int statusCode = 0;
    auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), statusCode);
    if (error != std::errc() || end != digits.data() + digits.size())
        return 0;

    return statusCode;
}

Formats::Bytes BytesOf(std::string_view text)
{
    return Formats::Bytes(text.begin(), text.end());
}

}

OwnServer::OwnServer(Ios::SocketService& socketService, std::string_view address)
    : sockets(socketService)
    , host(address)
{
    std::size_t colon = address.rfind(':');
    if (colon == std::string_view::npos)
        return;

    std::optional<std::uint16_t> givenPort = PortOf(address.substr(colon + 1));
    if (!givenPort)
        return;

    host = std::string(address.substr(0, colon));
    port = *givenPort;
}

OwnServer::Connection OwnServer::Open()
{
    if (host.empty())
        return {ServerAnswer::NameNotFound, 0};

    Ios::Result<std::vector<Ios::Ipv4Address>> addresses = sockets.GetAddressInfo(host);
    if (addresses.Failed() || addresses.Value().empty())
        return {ServerAnswer::NameNotFound, 0};

    Ios::Result<Ios::SocketId> socket = sockets.CreateSocket(Ios::SocketType::Stream, 0);
    if (socket.Failed())
        return {ServerAnswer::NoAnswer, 0};

    {
        std::lock_guard<std::mutex> lock(mutex);
        openSocket = socket.Value();
        if (isCancelled)
        {
            openSocket.reset();
            sockets.Close(socket.Value());
            return {ServerAnswer::NoAnswer, 0};
        }
    }

    if (sockets.Connect(socket.Value(), {addresses.Value().front(), port}).Failed())
    {
        Close(socket.Value());
        return {ServerAnswer::NoAnswer, 0};
    }

    return {ServerAnswer::Answered, socket.Value()};
}

// Cancel may have closed the socket already, and its number may belong to another socket by now.
void OwnServer::Close(Ios::SocketId socket)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (openSocket != socket)
        return;

    openSocket.reset();
    sockets.Close(socket);
}

void OwnServer::Cancel()
{
    std::lock_guard<std::mutex> lock(mutex);
    isCancelled = true;
    if (!openSocket)
        return;

    sockets.Close(*openSocket);
    openSocket.reset();
}

ServerAnswer OwnServer::Reach()
{
    Connection connection = Open();
    if (connection.answer == ServerAnswer::Answered)
        Close(connection.socket);

    return connection.answer;
}

std::string OwnServer::ReceiveHeader(Ios::SocketId socket)
{
    std::string reply;
    while (reply.find(HeaderEnd) == std::string::npos && reply.size() < MaximumHeaderSize)
    {
        Ios::Result<Formats::Bytes> received = sockets.ReceiveFrom(socket, ReceiveChunkSize);
        if (received.Failed() || received.Value().empty())
            break;

        reply.append(received.Value().begin(), received.Value().end());
    }
    return reply;
}

ServerReply OwnServer::Ask(std::string_view path)
{
    ServerReply reply;
    Connection connection = Open();
    reply.answer = connection.answer;
    if (connection.answer != ServerAnswer::Answered)
        return reply;

    std::string request = std::format("GET {} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", path, host);
    if (sockets.SendTo(connection.socket, BytesOf(request), nullptr).Failed())
    {
        Close(connection.socket);
        reply.answer = ServerAnswer::NoAnswer;
        return reply;
    }

    std::string received = ReceiveHeader(connection.socket);
    Close(connection.socket);

    std::size_t statusLineEnd = received.find(LineEnd);
    reply.statusCode = StatusCodeOf(std::string_view(received).substr(0, statusLineEnd));
    if (statusLineEnd != std::string::npos)
        reply.header = received.substr(statusLineEnd + LineEnd.size());

    return reply;
}

ServerContact::ServerContact(Ios::SocketService& sockets, std::string_view address)
    : server(sockets, address)
    , pendingAnswer(std::async(std::launch::async, [this] { return server.Reach(); }))
{
}

ServerContact::~ServerContact()
{
    server.Cancel();
}

std::optional<ServerAnswer> ServerContact::Answer()
{
    if (answer)
        return answer;

    if (pendingAnswer.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;

    answer = pendingAnswer.get();
    return answer;
}

}
