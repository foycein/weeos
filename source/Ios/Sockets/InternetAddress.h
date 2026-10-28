#pragma once

#include "Ios/Network/Ipv4Address.h"

#include <optional>
#include <string_view>

namespace Ios
{

// The text forms of an address that the socket stack of the console accepts (what C calls inet_aton). One to four numbers with
// dots between them. Each number is decimal, octal (it starts with 0) or hexadecimal (it starts with 0x). The last number fills
// all the bytes that are left: "127.1" is 127.0.0.1 and "0xC0A80101" is 192.168.1.1.
//
// The text after the address may be anything but a letter or a digit, so "10.0.0.1 and more" is accepted.
// Nothing is accepted when a number is missing, when a number in front of a dot is above 255, or when the last number does not fit.
// The original has two habits that are kept: a digit above 7 in an octal number counts, and a fourth number above 255 loses its upper bits.
std::optional<Ipv4Address> ParseInternetAddress(std::string_view text);

}
