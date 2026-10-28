#pragma once

#include "Ios/Result.h"

#include <compare>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Ios
{

// A path on the NAND, for example "/title/00010001/48414541/data". It always starts at the root.
//
// A path can be parsed when it has the shape the file system accepts: at most 63 characters, names made of
// printable characters, no empty name, no slash at the end. The limit of 12 characters per name and the depth limit
// are checked by the calls that create entries, because the console looks other paths up and finds nothing.
//
// Paths are ordered name by name. So everything inside a directory comes right after the directory.
class NandPath
{
public:
    static NandPath Root();
    static Result<NandPath> Parse(std::string_view text);

    bool IsRoot() const;

    // The number of names. The root has none.
    std::size_t Depth() const;

    // The last name. Empty for the root.
    std::string_view Name() const;

    // The directory that holds this entry. The parent of the root is the root.
    NandPath Parent() const;

    Result<NandPath> Child(std::string_view name) const;

    // The same path with every name cut to at most this many characters.
    NandPath WithNamesCutAt(std::size_t characterCount) const;

    // True when this path is the directory itself or lies anywhere below it.
    bool IsInside(const NandPath& directory) const;

    // The path of an entry after its directory moved: the names of oldDirectory at the start of this path are replaced by newDirectory.
    // The new path is not checked against the limits, as the console does not check an entry that moves with its directory.
    NandPath AfterMove(const NandPath& oldDirectory, const NandPath& newDirectory) const;

    std::string ToString() const;

    auto operator<=>(const NandPath&) const = default;

private:
    std::vector<std::string> names;
};

}
