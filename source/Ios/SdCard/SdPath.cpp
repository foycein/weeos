#include "Ios/SdCard/SdPath.h"

#include "Ios/FileSystem/HostNames.h"

namespace Ios
{

namespace
{

constexpr char PathSeparator = '/';
constexpr std::string_view CharactersFatRejects = "\\/:*?\"<>|";
constexpr unsigned char FirstPrintableByte = 0x20;

bool IsRejectedCharacter(char character)
{
    if (static_cast<unsigned char>(character) < FirstPrintableByte)
        return true;

    return CharactersFatRejects.find(character) != std::string_view::npos;
}

bool IsValidName(std::string_view name)
{
    if (name.empty() || name.size() > MaximumSdNameLength)
        return false;

    if (name == "." || name == "..")
        return false;

    if (EndsWithDotOrSpace(name))
        return false;

    for (char character : name)
    {
        if (IsRejectedCharacter(character))
            return false;
    }

    return !IsReservedDeviceName(name);
}

}

SdPath SdPath::Root()
{
    return SdPath();
}

Result<SdPath> SdPath::Parse(std::string_view text)
{
    if (text.empty() || text.front() != PathSeparator)
        return IosError::InvalidArgument;

    SdPath path;
    std::string_view rest = text.substr(1);
    if (rest.empty())
        return path;

    while (true)
    {
        std::size_t separatorPosition = rest.find(PathSeparator);
        std::string_view name = rest.substr(0, separatorPosition);
        if (!IsValidName(name))
            return IosError::InvalidArgument;

        path.names.emplace_back(name);
        if (separatorPosition == std::string_view::npos)
            return path;

        rest = rest.substr(separatorPosition + 1);
    }
}

bool SdPath::IsRoot() const
{
    return names.empty();
}

std::string_view SdPath::Name() const
{
    if (names.empty())
        return std::string_view();

    return names.back();
}

SdPath SdPath::Parent() const
{
    SdPath parent = *this;
    if (!parent.names.empty())
        parent.names.pop_back();

    return parent;
}

std::string SdPath::ToString() const
{
    if (names.empty())
        return std::string(1, PathSeparator);

    std::string text;
    for (const std::string& name : names)
    {
        text += PathSeparator;
        text += name;
    }
    return text;
}

std::filesystem::path SdPath::ToHostPath(const std::filesystem::path& cardFolder) const
{
    std::filesystem::path hostPath = cardFolder;
    for (const std::string& name : names)
    {
        std::u8string utf8Name(reinterpret_cast<const char8_t*>(name.data()), name.size());
        hostPath /= std::filesystem::path(utf8Name);
    }
    return hostPath;
}

}
