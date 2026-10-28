#pragma once

#include <cstdint>

namespace Ios
{

enum class AccessRights : std::uint8_t
{
    None = 0,
    Read = 1,
    Write = 2,
    ReadWrite = 3,
};

// True when the granted rights contain every right that is required.
constexpr bool Includes(AccessRights granted, AccessRights required)
{
    std::uint8_t grantedBits = static_cast<std::uint8_t>(granted);
    std::uint8_t requiredBits = static_cast<std::uint8_t>(required);
    return (grantedBits & requiredBits) == requiredBits;
}

// What the owner of an entry, the members of its group and everybody else may do with it.
struct EntryPermissions
{
    AccessRights owner;
    AccessRights group;
    AccessRights other;

    bool operator==(const EntryPermissions&) const = default;
};

}
