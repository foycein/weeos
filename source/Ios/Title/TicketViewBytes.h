#pragma once

#include "Formats/Bytes.h"
#include "Ios/Result.h"
#include "Ios/Title/TicketView.h"

#include <cstddef>

namespace Ios
{

// The ticket view as the console passes it between programs and keeps it in /sys/launch.sys: 0xD8 bytes, all numbers big endian.
//
//   0x00  the format version of the ticket, then three zero bytes
//   0x04  ticket id (8)    0x0C  console id (4)    0x10  title id (8)
//   0x18  access mask (2)  0x1A  title version (2)
//   0x1C  permitted titles mask (4)    0x20  permit mask (4)
//   0x24  export flag (1), then one zero byte where the ticket has the common key index
//   0x26  48 reserved bytes
//   0x56  content access mask (64)     then two zero bytes
//   0x98  the 8 limits, 8 bytes each: the type, then the maximum usage
//
// This is the order in which ES copies a ticket of 0x2A4 bytes into a view (ES 0x20106DE0 in boot2, 0x20107478 in IOS80).
constexpr std::size_t TicketViewSize = 0xD8;

Formats::Bytes WriteTicketView(const TicketView& view);

// The view in these bytes. The caller names the error for bytes that are not a view: ShortRead for a file of the NAND,
// TicketInvalid for a view that a program handed over.
Result<TicketView> ParseTicketView(Formats::ByteView bytes, IosError errorForUnreadableBytes);

}
