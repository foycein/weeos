#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

// One entry of an index group of a NintendoWare resource file (BRRES): its name and where its data starts, counted from the start of
// the bytes the group was read from.
struct ResourceEntry
{
    std::string name;
    std::size_t offset;
};

// The entries of the index group at the offset. The first entry of a group is the group's own root and is left out.
// Throws a FormatError when the group does not fit into the bytes.
std::vector<ResourceEntry> ReadIndexGroup(ByteView bytes, std::size_t groupOffset);

// Where an entry of a BRRES file lives, for example the model "graund_model" in the group "3DModels(NW4R)" of the root. Nothing when
// the file has no such group or entry.
std::optional<std::size_t> FindResource(ByteView brres, std::string_view groupName, std::string_view entryName);

}
