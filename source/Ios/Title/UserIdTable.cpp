#include "Ios/Title/UserIdTable.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>

namespace Ios
{

namespace
{

constexpr std::size_t EntrySize = 12;

UserIdEntry ReadEntry(Formats::ByteReader& reader)
{
    UserIdEntry entry;
    entry.titleId = TitleId::FromValue(reader.ReadUint64());
    entry.userId = reader.ReadUint32();
    return entry;
}

}

Result<UserIdTable> UserIdTable::Read(const KernelFiles& files)
{
    UserIdTable table;

    Result<Formats::Bytes> bytes = files.ReadFile(UserIdFilePath());
    if (bytes.Failed() && bytes.Error() == IosError::NotFound)
    {
        table.entries.push_back({SystemMenuTitleId, SystemMenuUserId});
        table.hasChanged = true;
        return table;
    }

    if (bytes.Failed())
        return bytes.Error();

    bool endsInTheMiddleOfAnEntry = bytes.Value().size() % EntrySize != 0;
    if (endsInTheMiddleOfAnEntry)
        return IosError::ShortRead;

    Formats::ByteReader reader(bytes.Value(), "User id table");
    while (!reader.IsAtEnd())
        table.entries.push_back(ReadEntry(reader));

    return table;
}

std::optional<std::uint32_t> UserIdTable::Find(TitleId titleId) const
{
    auto found = std::find_if(entries.begin(), entries.end(),
        [titleId](const UserIdEntry& entry) { return entry.titleId == titleId; });
    if (found == entries.end())
        return std::nullopt;

    return found->userId;
}

std::uint32_t UserIdTable::FindOrAdd(TitleId titleId)
{
    std::optional<std::uint32_t> existingUserId = Find(titleId);
    if (existingUserId)
        return *existingUserId;

    std::uint32_t newUserId = entries.empty() ? SystemMenuUserId : entries.back().userId + 1;
    entries.push_back({titleId, newUserId});
    hasChanged = true;
    return newUserId;
}

bool UserIdTable::HasChanged() const
{
    return hasChanged;
}

Result<void> UserIdTable::Write(KernelFiles& files)
{
    Formats::ByteWriter writer;
    for (const UserIdEntry& entry : entries)
    {
        writer.WriteUint64(entry.titleId.ToValue());
        writer.WriteUint32(entry.userId);
    }

    Result<void> written = files.WriteFile(UserIdFilePath(), writer.GetBytes());
    if (written.Failed())
        return written;

    hasChanged = false;
    return Result<void>();
}

}
