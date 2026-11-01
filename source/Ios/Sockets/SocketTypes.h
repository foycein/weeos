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

// The events of a poll entry, as the stack numbers them (Poll 0x13B51850 and the TCP state test 0x13B4EC18 of IOS80's SO).
constexpr std::uint16_t PollReadable = 0x0001;
constexpr std::uint16_t PollReadableUrgent = 0x0002;
constexpr std::uint16_t PollWritable = 0x0008;
constexpr std::uint16_t PollError = 0x0020;
constexpr std::uint16_t PollHungUp = 0x0040;
constexpr std::uint16_t PollInvalid = 0x0080;

// A socket and what a program waits for on it.
struct PollEntry
{
    SocketId socket;
    std::uint16_t requestedEvents;
    std::uint16_t happenedEvents;
};

}
