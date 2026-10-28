#pragma once

#include "Formats/Sha1.h"
#include "Ios/Result.h"
#include "Ios/Title/KernelFiles.h"

#include <optional>
#include <string>
#include <vector>

namespace Ios
{

// One content that several titles share: the name of its file in /shared1 and its SHA-1 hash.
struct SharedContentEntry
{
    std::string name;
    Formats::Sha1Digest hash;
};

// The file /shared1/content.map. A content with the "shared" type is not kept in the folder of its title.
// The title metadata names it by its hash, and this map says which file in /shared1 holds it.
// Every entry in the file is 28 bytes: the name as eight characters, then the hash.
class SharedContentMap
{
public:
    // A NAND without the file has no shared contents.
    static Result<SharedContentMap> Read(const KernelFiles& files);

    const std::vector<SharedContentEntry>& Entries() const;

    // The name of the file that holds the content with this hash, or nothing when no title has installed it.
    std::optional<std::string> FindName(const Formats::Sha1Digest& hash) const;

private:
    std::vector<SharedContentEntry> entries;
};

}
