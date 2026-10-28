#pragma once

#include "Ios/FileSystem/EntryAttributes.h"

#include <cstdint>
#include <optional>

namespace Ios
{

// The kind of an entry and the permissions of the owner, the group and everybody else.
struct EntryMode
{
    EntryKind kind;
    EntryPermissions permissions;

    bool operator==(const EntryMode&) const = default;
};

// The console keeps all of it in one byte, the mode byte of an entry in the file system tree:
//
//     bits 7 and 6   the permissions of the owner
//     bits 5 and 4   the permissions of the group
//     bits 3 and 2   the permissions of everybody else
//     bits 1 and 0   the kind: 1 is a file, 2 is a directory
//
// The two bits of a permission are the right to read (bit 0) and the right to write (bit 1).
// Gives back nothing when the kind bits mean neither a file nor a directory.
std::optional<EntryMode> DecodeEntryMode(std::uint8_t modeByte);

std::uint8_t EncodeEntryMode(const EntryMode& mode);

}
