#pragma once

#include "Ios/Sockets/SocketService.h"

#include <cstdint>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace SystemMenu
{

// WeeOS's own server takes the place of every Nintendo service: the Shop, the Forecast, the News, the connection test.
// Until it exists its address is empty, and every program shows the error of a server that cannot be
// reached, as the originals do when the Internet works and the server does not answer.
//
// What a try to reach it found.
enum class ServerAnswer
{
    // No address is set, or the name of the server is not known: the lookup of the name failed.
    NameNotFound,

    // The server did not answer: nobody listens at the address, or it cannot be reached.
    NoAnswer,

    // The server answered.
    Answered,
};

// What the server said to a request: its status code and the lines of its header. The code is 0 when the reply is not HTTP.
struct ServerReply
{
    ServerAnswer answer = ServerAnswer::NoAnswer;
    int statusCode = 0;
    std::string header;
};

constexpr std::uint16_t DefaultServerPort = 80;

// WeeOS's own server, reached through the sockets of IOS as a program of the console reaches its servers. The address is a host
// name or an IPv4 address, with ":port" when the port is not 80.
//
// Every call blocks until the server answered or the try failed; Cancel ends it from another thread.
class OwnServer
{
public:
    // The sockets must outlive this object, and the program has started them (StartNetwork).
    OwnServer(Ios::SocketService& sockets, std::string_view address);

    // Looks the name of the server up and opens a connection to it, then closes it again.
    ServerAnswer Reach();

    // Asks for the page at the path (HTTP GET) and reads the reply up to the end of its header.
    ServerReply Ask(std::string_view path);

    // Ends the try that waits, which then finds no answer, and every later one.
    void Cancel();

private:
    struct Connection
    {
        ServerAnswer answer;
        Ios::SocketId socket;
    };

    Connection Open();
    void Close(Ios::SocketId socket);
    std::string ReceiveHeader(Ios::SocketId socket);

    Ios::SocketService& sockets;
    std::string host;
    std::uint16_t port = DefaultServerPort;

    std::mutex mutex;
    std::optional<Ios::SocketId> openSocket;
    bool isCancelled = false;
};

// A try to reach WeeOS's own server that runs beside the program, so that the picture goes on while it waits. A program that leaves
// before the answer came ends the try.
class ServerContact
{
public:
    // Starts at once. The sockets must outlive this object.
    ServerContact(Ios::SocketService& sockets, std::string_view address);
    ~ServerContact();

    ServerContact(const ServerContact&) = delete;
    ServerContact& operator=(const ServerContact&) = delete;

    // Nothing while the try runs.
    std::optional<ServerAnswer> Answer();

private:
    OwnServer server;
    std::future<ServerAnswer> pendingAnswer;
    std::optional<ServerAnswer> answer;
};

}
