#include "NandImporter.h"

#include "FolderFiles.h"
#include "NandFolderLayout.h"
#include "PersonalSettings.h"
#include "WindowsNames.h"

#include "Formats/NandTitles.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <set>
#include <stdexcept>

namespace
{

constexpr std::string_view RootPath = "/";
constexpr std::string_view FileKindText = "file";
constexpr std::string_view DirectoryKindText = "directory";

// The first lines of the index file. They are only for a person who opens it: the reader skips every line that starts with #.
constexpr std::string_view IndexHeaderText =
    "# WeeOS NAND index: the owner, group, permissions and flags of every NAND entry.\n"
    "# One line per entry, columns separated by tabs: path, kind, owner id, group id, permissions (owner group other),\n"
    "# flags, link number and the name on the PC when it differs from the NAND name.\n";

// A directory lists its entries from the highest link number down, so the first entry of the file system gets the highest one.
// The root directory has the number 0.
constexpr std::uint64_t RootLinkNumber = 0;

void CountEntry(RuleTotals& totals, const Formats::NandEntry& entry)
{
    if (entry.kind == Formats::NandEntryKind::Directory)
    {
        ++totals.directoryCount;
        return;
    }

    ++totals.fileCount;
    totals.byteCount += entry.size;
}

// Every title has one title metadata file, so counting these files counts the titles.
void CountTitle(ImportPlan& plan, const Formats::NandEntry& entry, bool isImported)
{
    bool isTitleMetadata = entry.path.starts_with("/title/") && entry.path.ends_with("/content/title.tmd");
    if (!isTitleMetadata)
        return;

    if (isImported)
        ++plan.importedTitleCount;
    else
        ++plan.skippedTitleCount;
}

void RequireParentsImported(const ImportPlan& plan)
{
    std::set<std::string_view> importedPaths = {RootPath};
    for (const Formats::NandEntry* entry : plan.importedEntries)
    {
        if (!importedPaths.contains(Formats::ParentPathOf(entry->path)))
            throw std::runtime_error(std::format("The rules import {} but not the directory it lies in", entry->path));

        importedPaths.insert(entry->path);
    }
}

void RequireNamesWindowsCanStore(const ImportPlan& plan)
{
    std::set<std::string> lowerCasePaths;
    for (const Formats::NandEntry* entry : plan.importedEntries)
    {
        if (!CanBeStoredOnWindows(entry->name))
            throw std::runtime_error(std::format("Windows cannot keep the name \"{}\" of {}", entry->name, entry->path));

        bool isNewPath = lowerCasePaths.insert(LowerCaseName(entry->path)).second;
        if (!isNewPath)
            throw std::runtime_error(std::format("{} differs from another imported entry only in capital and small letters", entry->path));
    }
}

// For example "rw rw r-": the rights of the owner, the group and everybody else.
std::string PermissionsText(const Formats::NandEntry& entry)
{
    return std::format("{} {} {}", Formats::RightsText(entry.ownerRights), Formats::RightsText(entry.groupRights),
        Formats::RightsText(entry.otherRights));
}

std::string IndexLine(const Formats::NandEntry& entry, std::uint64_t linkNumber)
{
    std::string_view kindText = entry.kind == Formats::NandEntryKind::Directory ? DirectoryKindText : FileKindText;
    return std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\n", entry.path, kindText, entry.userId, entry.groupId,
        PermissionsText(entry), static_cast<unsigned int>(entry.attributes), linkNumber);
}

// The lines are in the order of the paths, name by name, as the NAND index keeps them.
std::string BuildIndexText(const Formats::NandEntry& root, const std::vector<const Formats::NandEntry*>& importedEntries)
{
    struct IndexLineWithNames
    {
        std::vector<std::string> names;
        std::string text;
    };

    std::vector<IndexLineWithNames> lines;
    lines.push_back({SplitPath(root.path), IndexLine(root, RootLinkNumber)});

    std::uint64_t linkNumber = importedEntries.size();
    for (const Formats::NandEntry* entry : importedEntries)
    {
        lines.push_back({SplitPath(entry->path), IndexLine(*entry, linkNumber)});
        --linkNumber;
    }

    std::sort(lines.begin(), lines.end(),
        [](const IndexLineWithNames& line, const IndexLineWithNames& otherLine) { return line.names < otherLine.names; });

    std::string text(IndexHeaderText);
    for (const IndexLineWithNames& line : lines)
        text += line.text;

    return text;
}

// What is written for a file: the bytes the NAND holds, except for a system configuration, which loses the settings of the owner, and
// the network configuration, which holds WeeOS's connection.
Formats::Bytes ContentToImport(Formats::NandFileSystem& source, const ImportPlan& plan, const Formats::NandEntry& file)
{
    Formats::Bytes content = source.ReadFile(file);

    ImportAction action = plan.rules[FindRuleNumber(plan.rules, file.path)].action;
    if (action == ImportAction::ImportWithWeeOSConnection)
        return MakeWeeOSNetworkConfigurationFile();

    if (action != ImportAction::ImportWithPersonalSettingsReset)
        return content;

    return ResetPersonalSettingsOfFile(content);
}

}

ImportPlan PlanSystemImport(Formats::NandFileSystem& source)
{
    Formats::NandTitles titles(source);
    return PlanImport(source, BuildImportRules(FindHomebrewChannelIds(titles)));
}

ImportPlan PlanCompleteImport(Formats::NandFileSystem& source)
{
    return PlanImport(source, BuildCompleteImportRules());
}

ImportPlan PlanImport(Formats::NandFileSystem& source, std::vector<ImportRule> rules)
{
    ImportPlan plan;
    plan.rules = std::move(rules);
    plan.totals.resize(plan.rules.size());

    for (const Formats::NandEntry& entry : source.Entries())
    {
        if (entry.path == RootPath)
            continue;

        std::size_t ruleNumber = FindRuleNumber(plan.rules, entry.path);
        CountEntry(plan.totals[ruleNumber], entry);

        bool isImported = IsImported(plan.rules[ruleNumber].action);
        if (isImported)
            plan.importedEntries.push_back(&entry);

        CountTitle(plan, entry, isImported);
    }

    RequireParentsImported(plan);
    RequireNamesWindowsCanStore(plan);
    return plan;
}

void WriteImport(Formats::NandFileSystem& source, const ImportPlan& plan, const std::filesystem::path& targetFolder)
{
    PrepareEmptyFolder(targetFolder);

    for (const Formats::NandEntry* entry : plan.importedEntries)
    {
        std::filesystem::path hostPath = targetFolder / entry->path.substr(RootPath.size());
        if (entry->kind == Formats::NandEntryKind::Directory)
            std::filesystem::create_directory(hostPath);
        else
            WriteBytesToFile(hostPath, ContentToImport(source, plan, *entry));
    }

    // The index comes last: a folder without an index is an empty NAND, so an import that stops half way leaves nothing half done.
    const Formats::NandEntry& root = source.Entries().front();
    std::string indexText = BuildIndexText(root, plan.importedEntries);
    WriteBytesToFile(targetFolder / IndexFileName, Formats::Bytes(indexText.begin(), indexText.end()));
}
