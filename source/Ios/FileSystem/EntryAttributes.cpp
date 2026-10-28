#include "Ios/FileSystem/EntryAttributes.h"

namespace Ios
{

namespace
{

AccessRights GrantedRights(const EntryAttributes& attributes, const Caller& caller)
{
    if (caller.userId == attributes.ownerId)
        return attributes.permissions.owner;

    if (caller.groupId == attributes.groupId)
        return attributes.permissions.group;

    return attributes.permissions.other;
}

}

bool HasRights(const EntryAttributes& attributes, const Caller& caller, AccessRights required)
{
    if (caller.userId == SuperUserId)
        return true;

    return Includes(GrantedRights(attributes, caller), required);
}

}
