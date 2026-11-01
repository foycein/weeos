#include "Ios/FileSystem/NandPath.h"

#include "Ios/FileSystem/NandLimits.h"

#include <algorithm>

namespace Ios
{

namespace
{

constexpr char PathSeparator = '/';
constexpr char FirstPrintableCharacter = ' ';
constexpr char LastPrintableCharacter = '~';

bool IsPrintable(char character)
{
    return character >= FirstPrintableCharacter && character <= LastPrintableCharacter;
}

bool IsNameCharacter(char character)
{
    return character != PathSeparator && IsPrintable(character);
}

bool IsValidName(std::string_view name)
{
    if (name.empty())
        return false;

    return std::all_of(name.begin(), name.end(), IsNameCharacter);
}

}

NandPath NandPath::Root()
{
    return NandPath();
}

Result<NandPath> NandPath::Parse(std::string_view text)
{
    if (text.empty() || text.front() != PathSeparator)
        return IosError::InvalidArgument;

    if (text.size() > MaximumPathLength)
        return IosError::InvalidArgument;

    if (text.size() == 1)
        return Root();

    if (text.back() == PathSeparator)
        return IosError::InvalidArgument;

    NandPath path;
    std::string_view remainder = text.substr(1);
    while (true)
    {
        std::size_t separatorIndex = remainder.find(PathSeparator);
        std::string_view name = remainder.substr(0, separatorIndex);
        if (!IsValidName(name))
            return IosError::InvalidArgument;

        path.names.emplace_back(name);
        if (separatorIndex == std::string_view::npos)
            return path;

        remainder.remove_prefix(separatorIndex + 1);
    }
}

bool NandPath::IsRoot() const
{
    return names.empty();
}

std::size_t NandPath::Depth() const
{
    return names.size();
}

std::string_view NandPath::Name() const
{
    if (IsRoot())
        return std::string_view();

    return names.back();
}

NandPath NandPath::Parent() const
{
    NandPath parent = *this;
    if (!parent.IsRoot())
        parent.names.pop_back();

    return parent;
}

Result<NandPath> NandPath::Child(std::string_view name) const
{
    if (!IsValidName(name))
        return IosError::InvalidArgument;

    NandPath child = *this;
    child.names.emplace_back(name);
    if (child.ToString().size() > MaximumPathLength)
        return IosError::InvalidArgument;

    return child;
}

NandPath NandPath::WithNamesCutAt(std::size_t characterCount) const
{
    NandPath cutPath;
    for (const std::string& name : names)
        cutPath.names.push_back(name.substr(0, characterCount));

    return cutPath;
}

bool NandPath::IsInside(const NandPath& directory) const
{
    if (names.size() < directory.names.size())
        return false;

    return std::equal(directory.names.begin(), directory.names.end(), names.begin());
}

NandPath NandPath::AfterMove(const NandPath& oldDirectory, const NandPath& newDirectory) const
{
    NandPath movedPath = newDirectory;
    movedPath.names.insert(movedPath.names.end(), names.begin() + oldDirectory.names.size(), names.end());
    return movedPath;
}

std::string NandPath::ToString() const
{
    if (IsRoot())
        return std::string(1, PathSeparator);

    std::string text;
    for (const std::string& name : names)
    {
        text += PathSeparator;
        text += name;
    }
    return text;
}

}
