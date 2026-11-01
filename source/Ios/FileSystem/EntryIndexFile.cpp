#include "Ios/FileSystem/EntryIndexFile.h"

#include "Ios/FileSystem/HostNames.h"

#include <charconv>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Ios
{

namespace
{

constexpr char ColumnSeparator = '\t';
constexpr char LineEnd = '\n';
constexpr char CarriageReturn = '\r';
constexpr char CommentMarker = '#';
constexpr std::string_view FileKindText = "file";
constexpr std::string_view DirectoryKindText = "directory";
constexpr std::size_t PathColumn = 0;
constexpr std::size_t KindColumn = 1;
constexpr std::size_t OwnerIdColumn = 2;
constexpr std::size_t GroupIdColumn = 3;
constexpr std::size_t PermissionsColumn = 4;
constexpr std::size_t FlagsColumn = 5;
constexpr std::size_t LinkNumberColumn = 6;
constexpr std::size_t HostNameColumn = 7;
constexpr std::size_t ColumnCountWithoutHostName = 7;
constexpr std::size_t ColumnCountWithHostName = 8;

// The permissions are written as two letters for each of the owner, the group and everybody else, for example "rw rw r-".
constexpr std::size_t RightsTextLength = 2;
constexpr std::size_t ReadLetterOffset = 0;
constexpr std::size_t WriteLetterOffset = 1;
constexpr std::size_t PermissionsTextLength = 8;
constexpr std::size_t OwnerRightsOffset = 0;
constexpr std::size_t FirstSeparatorOffset = 2;
constexpr std::size_t GroupRightsOffset = 3;
constexpr std::size_t SecondSeparatorOffset = 5;
constexpr std::size_t OtherRightsOffset = 6;
constexpr char ReadLetter = 'r';
constexpr char WriteLetter = 'w';
constexpr char NoRightLetter = '-';
constexpr char RightsSeparator = ' ';

constexpr std::string_view HeaderText =
    "# WeeOS NAND index: the owner, group, permissions and flags of every NAND entry.\n"
    "# One line per entry, columns separated by tabs: path, kind, owner id, group id, permissions (owner group other),\n"
    "# flags, link number and the name on the PC when it differs from the NAND name.\n";

std::string RightsText(AccessRights rights)
{
    std::string text(RightsTextLength, NoRightLetter);
    if (Includes(rights, AccessRights::Read))
        text[ReadLetterOffset] = ReadLetter;

    if (Includes(rights, AccessRights::Write))
        text[WriteLetterOffset] = WriteLetter;

    return text;
}

std::string PermissionsText(const EntryPermissions& permissions)
{
    std::string text = RightsText(permissions.owner);
    text += RightsSeparator;
    text += RightsText(permissions.group);
    text += RightsSeparator;
    text += RightsText(permissions.other);
    return text;
}

std::string FormatLine(const NandPath& path, const EntryRecord& record)
{
    std::vector<std::string> columns;
    columns.push_back(path.ToString());
    columns.push_back(std::string(record.kind == EntryKind::Directory ? DirectoryKindText : FileKindText));
    columns.push_back(std::to_string(record.attributes.ownerId));
    columns.push_back(std::to_string(record.attributes.groupId));
    columns.push_back(PermissionsText(record.attributes.permissions));
    columns.push_back(std::to_string(record.attributes.flags));
    columns.push_back(std::to_string(record.linkNumber));

    bool hasOtherHostName = !path.IsRoot() && record.hostName != path.Name();
    if (hasOtherHostName)
        columns.push_back(record.hostName);

    std::string line = columns.front();
    for (std::size_t index = 1; index < columns.size(); ++index)
    {
        line += ColumnSeparator;
        line += columns[index];
    }
    line += LineEnd;
    return line;
}

std::vector<std::string_view> SplitColumns(std::string_view line)
{
    std::vector<std::string_view> columns;
    while (true)
    {
        std::size_t separatorIndex = line.find(ColumnSeparator);
        columns.push_back(line.substr(0, separatorIndex));
        if (separatorIndex == std::string_view::npos)
            return columns;

        line.remove_prefix(separatorIndex + 1);
    }
}

template <typename Number>
std::optional<Number> ParseNumber(std::string_view text)
{
    std::uint64_t number = 0;
    const char* textEnd = text.data() + text.size();
    std::from_chars_result parsed = std::from_chars(text.data(), textEnd, number);
    if (parsed.ec != std::errc() || parsed.ptr != textEnd)
        return std::nullopt;

    if (number > std::numeric_limits<Number>::max())
        return std::nullopt;

    return static_cast<Number>(number);
}

std::optional<AccessRights> ParseRights(std::string_view text)
{
    char readLetter = text[ReadLetterOffset];
    char writeLetter = text[WriteLetterOffset];
    bool canRead = readLetter == ReadLetter;
    bool canWrite = writeLetter == WriteLetter;
    if (!canRead && readLetter != NoRightLetter)
        return std::nullopt;

    if (!canWrite && writeLetter != NoRightLetter)
        return std::nullopt;

    if (canRead && canWrite)
        return AccessRights::ReadWrite;

    if (canRead)
        return AccessRights::Read;

    if (canWrite)
        return AccessRights::Write;

    return AccessRights::None;
}

std::optional<EntryPermissions> ParsePermissions(std::string_view text)
{
    if (text.size() != PermissionsTextLength)
        return std::nullopt;

    if (text[FirstSeparatorOffset] != RightsSeparator || text[SecondSeparatorOffset] != RightsSeparator)
        return std::nullopt;

    std::optional<AccessRights> owner = ParseRights(text.substr(OwnerRightsOffset, RightsTextLength));
    std::optional<AccessRights> group = ParseRights(text.substr(GroupRightsOffset, RightsTextLength));
    std::optional<AccessRights> other = ParseRights(text.substr(OtherRightsOffset, RightsTextLength));
    if (!owner || !group || !other)
        return std::nullopt;

    return EntryPermissions{*owner, *group, *other};
}

std::optional<EntryKind> ParseKind(std::string_view text)
{
    if (text == FileKindText)
        return EntryKind::File;

    if (text == DirectoryKindText)
        return EntryKind::Directory;

    return std::nullopt;
}

struct ParsedLine
{
    NandPath path;
    EntryRecord record;
};

// Gives back nothing when the line is not a complete and valid entry.
std::optional<ParsedLine> ParseLine(std::string_view line)
{
    std::vector<std::string_view> columns = SplitColumns(line);
    bool hasHostName = columns.size() == ColumnCountWithHostName;
    if (columns.size() != ColumnCountWithoutHostName && !hasHostName)
        return std::nullopt;

    Result<NandPath> path = NandPath::Parse(columns[PathColumn]);
    std::optional<EntryKind> kind = ParseKind(columns[KindColumn]);
    std::optional<std::uint32_t> ownerId = ParseNumber<std::uint32_t>(columns[OwnerIdColumn]);
    std::optional<std::uint16_t> groupId = ParseNumber<std::uint16_t>(columns[GroupIdColumn]);
    std::optional<EntryPermissions> permissions = ParsePermissions(columns[PermissionsColumn]);
    std::optional<std::uint8_t> flags = ParseNumber<std::uint8_t>(columns[FlagsColumn]);
    std::optional<std::uint64_t> linkNumber = ParseNumber<std::uint64_t>(columns[LinkNumberColumn]);
    if (path.Failed() || !kind || !ownerId || !groupId || !permissions || !flags || !linkNumber)
        return std::nullopt;

    ParsedLine parsed;
    parsed.path = path.Value();
    parsed.record.kind = *kind;
    parsed.record.attributes = EntryAttributes{*ownerId, *groupId, *permissions, *flags};
    parsed.record.linkNumber = *linkNumber;
    parsed.record.length = 0;

    if (parsed.path.IsRoot())
    {
        if (hasHostName)
            return std::nullopt;

        return parsed;
    }

    if (!hasHostName)
    {
        parsed.record.hostName = std::string(parsed.path.Name());
        return parsed;
    }

    parsed.record.hostName = std::string(columns[HostNameColumn]);
    if (!IsValidHostName(parsed.record.hostName))
        return std::nullopt;

    return parsed;
}

bool HasParentDirectory(const EntryMap& entries, const NandPath& path)
{
    auto parent = entries.find(path.Parent());
    return parent != entries.end() && parent->second.kind == EntryKind::Directory;
}

bool IsConsistent(const EntryMap& entries)
{
    auto root = entries.find(NandPath::Root());
    if (root == entries.end() || root->second.kind != EntryKind::Directory)
        return false;

    for (const auto& [path, record] : entries)
    {
        if (!path.IsRoot() && !HasParentDirectory(entries, path))
            return false;
    }
    return true;
}

}

std::string WriteEntryIndex(const EntryMap& entries)
{
    std::string text(HeaderText);
    for (const auto& [path, record] : entries)
        text += FormatLine(path, record);

    return text;
}

Result<EntryMap> ReadEntryIndex(std::string_view text)
{
    EntryMap entries;
    while (!text.empty())
    {
        std::size_t lineEnd = text.find(LineEnd);
        std::string_view line = text.substr(0, lineEnd);
        if (lineEnd == std::string_view::npos)
            text = std::string_view();
        else
            text.remove_prefix(lineEnd + 1);

        if (!line.empty() && line.back() == CarriageReturn)
            line.remove_suffix(1);

        if (line.empty() || line.front() == CommentMarker)
            continue;

        std::optional<ParsedLine> parsed = ParseLine(line);
        if (!parsed)
            return IosError::NoFileSystem;

        bool isNewPath = entries.emplace(parsed->path, parsed->record).second;
        if (!isNewPath)
            return IosError::NoFileSystem;
    }

    if (!IsConsistent(entries))
        return IosError::NoFileSystem;

    return entries;
}

}
