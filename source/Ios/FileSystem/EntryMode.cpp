#include "Ios/FileSystem/EntryMode.h"

namespace Ios
{

namespace
{

constexpr std::uint8_t KindMask = 0x03;
constexpr std::uint8_t FileKindBits = 1;
constexpr std::uint8_t DirectoryKindBits = 2;

constexpr std::uint8_t PermissionMask = 0x03;
constexpr int OwnerShift = 6;
constexpr int GroupShift = 4;
constexpr int OtherShift = 2;

AccessRights ReadPermission(std::uint8_t modeByte, int shift)
{
    return static_cast<AccessRights>((modeByte >> shift) & PermissionMask);
}

std::uint8_t PermissionBits(AccessRights rights, int shift)
{
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(rights) << shift);
}

}

std::optional<EntryMode> DecodeEntryMode(std::uint8_t modeByte)
{
    EntryMode mode;
    mode.permissions.owner = ReadPermission(modeByte, OwnerShift);
    mode.permissions.group = ReadPermission(modeByte, GroupShift);
    mode.permissions.other = ReadPermission(modeByte, OtherShift);

    std::uint8_t kindBits = modeByte & KindMask;
    if (kindBits == FileKindBits)
        mode.kind = EntryKind::File;
    else if (kindBits == DirectoryKindBits)
        mode.kind = EntryKind::Directory;
    else
        return std::nullopt;

    return mode;
}

std::uint8_t EncodeEntryMode(const EntryMode& mode)
{
    std::uint8_t kindBits = mode.kind == EntryKind::File ? FileKindBits : DirectoryKindBits;

    return PermissionBits(mode.permissions.owner, OwnerShift)
        | PermissionBits(mode.permissions.group, GroupShift)
        | PermissionBits(mode.permissions.other, OtherShift)
        | kindBits;
}

}
