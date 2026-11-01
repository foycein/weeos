#pragma once

#include "Ios/Caller.h"
#include "Ios/FileSystem/EntryPermissions.h"

#include <cstdint>

namespace Ios
{

enum class EntryKind : std::uint8_t
{
    File,
    Directory,
};

// Who owns a file or a directory and what the others may do with it.
struct EntryAttributes
{
    std::uint32_t ownerId;
    std::uint16_t groupId;
    EntryPermissions permissions;

    // A byte the file system keeps for the programs and never looks at.
    std::uint8_t flags;

    bool operator==(const EntryAttributes&) const = default;
};

// The flags byte of the entries that the services make.
constexpr std::uint8_t NoEntryFlags = 0;

// True when the caller has all the required rights on an entry with these attributes.
// The owner gets the owner permissions. If the caller is not the owner, a member of the group gets the group permissions.
// Everybody else gets the other permissions. The super user always has all rights.
bool HasRights(const EntryAttributes& attributes, const Caller& caller, AccessRights required);

}
