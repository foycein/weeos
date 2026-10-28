#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Ios
{

constexpr std::size_t MacAddressSize = 6;

// The address of a network card, for example 58:BD:A3:81:17:4F is {0x58, 0xBD, 0xA3, 0x81, 0x17, 0x4F}.
using MacAddress = std::array<std::uint8_t, MacAddressSize>;

}
