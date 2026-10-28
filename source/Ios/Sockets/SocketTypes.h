#pragma once

#include "Ios/Network/Ipv4Address.h"

#include <cstdint>

namespace Ios
{

// The number of an open socket.
using SocketId = std::int32_t;

enum class SocketType : std::int32_t
{
    Stream = 1,
    Datagram = 2,
};

enum class ShutdownDirection : std::int32_t
{
    Receive = 0,
    Send = 1,
    Both = 2,
};

// Where a socket connects to or sends to. Only IPv4 addresses are rebuilt.
struct SocketAddress
{
    Ipv4Address address;
    std::uint16_t port;
};

// A socket and what a program waits for on it.
struct PollEntry
{
    SocketId socket;
    std::uint16_t requestedEvents;
    std::uint16_t happenedEvents;
};

}
