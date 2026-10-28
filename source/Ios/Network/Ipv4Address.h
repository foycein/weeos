#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Ios
{

// An IPv4 address as one number: the four parts a.b.c.d are 0xaabbccdd.
using Ipv4Address = std::uint32_t;

constexpr int BitsPerIpv4Part = 8;

constexpr Ipv4Address MakeIpv4Address(std::uint8_t first, std::uint8_t second, std::uint8_t third, std::uint8_t fourth)
{
    Ipv4Address address = first;
    address = (address << BitsPerIpv4Part) | second;
    address = (address << BitsPerIpv4Part) | third;
    return (address << BitsPerIpv4Part) | fourth;
}

// "192.168.1.40" becomes a number. Four parts with 1 to 3 digits each, every part at most 255. Nothing else is accepted.
std::optional<Ipv4Address> ParseIpv4Address(std::string_view text);

// The number as four parts with dots between them.
std::string FormatIpv4Address(Ipv4Address address);

}
