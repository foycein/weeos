#pragma once

#include "Ios/Result.h"
#include "Ios/Title/KernelFiles.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Ios
{

struct UserIdEntry
{
    TitleId titleId;
    std::uint32_t userId;
};

// The file /sys/uid.sys: the user id that every installed or launched title runs as. Every entry is 12 bytes: the title id, then the user id.
// The System Menu comes first with the user id 0x1000. Every title that comes later gets the user id after the last one in the file.
class UserIdTable
{
public:
    // A NAND without the file starts a new table that holds the System Menu.
    static Result<UserIdTable> Read(const KernelFiles& files);

    std::optional<std::uint32_t> Find(TitleId titleId) const;

    // The user id of the title. A title that has none is added to the table.
    std::uint32_t FindOrAdd(TitleId titleId);

    // True when the table differs from the file it was read from.
    bool HasChanged() const;

    Result<void> Write(KernelFiles& files);

private:
    std::vector<UserIdEntry> entries;
    bool hasChanged = false;
};

}
