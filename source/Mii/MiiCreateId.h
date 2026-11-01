#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <chrono>
#include <cstdint>

namespace Mii
{

constexpr std::size_t MacAddressSize = 6;

// The Mii id of a normal Mii made at this time: the number of four-second steps since the start of 2006, with the flag of a normal Mii.
std::uint32_t MakeMiiId(std::chrono::sys_seconds creationTime);

// The system id that the Miis of a console carry, made from the console's MAC address.
std::uint32_t MakeSystemId(const std::array<Formats::Byte, MacAddressSize>& macAddress);

}
