#pragma once

#include <cstdint>

namespace Ios
{

// Who asks a service for something. The file system compares it with the owner and the group of the entry.
struct Caller
{
    std::uint32_t userId;
    std::uint16_t groupId;

    bool operator==(const Caller&) const = default;
};

// The kernel and the title service run as this user. The file system lets it do everything.
constexpr std::uint32_t SuperUserId = 0;
constexpr std::uint16_t SuperUserGroupId = 0;
constexpr Caller SuperUserCaller = {SuperUserId, SuperUserGroupId};

}
