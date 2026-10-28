#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

struct U8Entry
{
    // The path from the root of the archive without a leading slash, for example "arc/blyt/title.brlyt".
    std::string path;

    bool isDirectory;

    // Where the data of a file lies inside the archive. Both are zero for a directory.
    std::size_t dataOffset;
    std::size_t dataSize;
};

// A U8 archive is a folder tree packed into one file. It has no compression and no encryption.
struct U8Archive
{
    Bytes bytes;
    std::vector<U8Entry> entries;

    // Returns the data of the file with this path (a leading slash is allowed), or nothing when there is no such file.
    // Paths are compared exactly, upper and lower case letters are different.
    // The data stays inside the archive, so it is only valid while the archive exists.
    std::optional<ByteView> FindFile(std::string_view path) const&;
    std::optional<ByteView> FindFile(std::string_view path) const&& = delete;

    ByteView FileData(const U8Entry& entry) const&;
    ByteView FileData(const U8Entry& entry) const&& = delete;
};

// True when the data starts with the tag of a U8 archive.
bool IsU8Archive(ByteView bytes);

U8Archive ReadU8Archive(ByteView bytes);

}
