#include "ImportRules.h"

#include "Formats/TitleMetadata.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace
{

constexpr char PathSeparator = '/';
constexpr std::string_view AnyName = "*";

// The titles with the high half 00000001 are IOS and the System Menu. Some IOS on a console are patched by homebrew tools and
// signed with zeros too, but they are part of the system and stay in.
constexpr int HighWordShift = 32;
constexpr std::uint64_t SystemTitleHighWord = 1;

bool IsZero(Formats::Byte value)
{
    return value == 0;
}

bool Covers(const ImportRule& rule, const std::vector<std::string>& pathNames)
{
    std::vector<std::string> patternNames = SplitPath(rule.pattern);
    if (rule.scope == ImportScope::OnlyThisEntry && patternNames.size() != pathNames.size())
        return false;

    if (patternNames.size() > pathNames.size())
        return false;

    for (std::size_t nameNumber = 0; nameNumber < patternNames.size(); ++nameNumber)
    {
        const std::string& patternName = patternNames[nameNumber];
        bool nameMatches = patternName == AnyName || patternName == pathNames[nameNumber];
        if (!nameMatches)
            return false;
    }
    return true;
}

ImportRule SkipEverythingInside(std::string pattern, std::string reason)
{
    return {std::move(pattern), ImportScope::EverythingInside, ImportAction::Skip, std::move(reason)};
}

ImportRule SkipOnlyThisEntry(std::string pattern, std::string reason)
{
    return {std::move(pattern), ImportScope::OnlyThisEntry, ImportAction::Skip, std::move(reason)};
}

ImportRule ImportEverythingInside(std::string pattern, std::string reason)
{
    return {std::move(pattern), ImportScope::EverythingInside, ImportAction::Import, std::move(reason)};
}

ImportRule ImportOnlyThisEntry(std::string pattern, std::string reason)
{
    return {std::move(pattern), ImportScope::OnlyThisEntry, ImportAction::Import, std::move(reason)};
}

// The fixed rules. Personal data stays out, and so does everything that is not clear.
std::vector<ImportRule> FixedImportRules()
{
    return {
        SkipEverythingInside("/shared2/menu", "Miis (FaceLib) and the settings of Virtual Console games: personal data"),
        SkipEverythingInside("/shared2/wc24", "Message Board mail and WiiConnect24 data: personal data"),
        ImportOnlyThisEntry("/shared2/sys/net", "the folder of the network configuration"),
        ImportOnlyThisEntry("/shared2/sys/net/02", "the folder of the network configuration of the IOS of today"),
        {"/shared2/sys/net/02/config.dat", ImportScope::OnlyThisEntry, ImportAction::ImportWithWeeOSConnection,
            "the network configuration: WeeOS's wired connection with automatic settings instead of the owner's networks"},
        SkipEverythingInside("/shared2/sys/net", "network configuration: the names, passwords and addresses of the owner's networks"),
        SkipEverythingInside("/title/00010000", "save data of disc games: personal data"),

        SkipEverythingInside("/title/00000001/00000100", "BC, the Wii side of the GameCube mode: the owner wants no GameCube"),
        SkipOnlyThisEntry("/ticket/00000001/00000100.tik", "ticket of BC"),
        SkipEverythingInside("/title/00000001/00000101", "MIOS, the GameCube mode of the console: the owner wants no GameCube"),
        SkipOnlyThisEntry("/ticket/00000001/00000101.tik", "ticket of MIOS"),

        ImportOnlyThisEntry("/title/00000001/00000002/data/setting.txt", "the facts about the console: serial number, model, region, video mode"),
        SkipEverythingInside("/title/*/*/data/*", "what a title keeps in its data folder: save data, caches, logs, played-game history, downloads"),

        // The Internet Channel is part of the system. The other downloaded channels are not.
        ImportOnlyThisEntry("/title/00010001", "the folder of the downloaded channels, which holds the Internet Channel"),
        ImportOnlyThisEntry("/ticket/00010001", "the folder of the tickets of the downloaded channels"),
        ImportEverythingInside("/title/00010001/48414445", "the Internet Channel, without its data (the rule above skips it)"),
        ImportOnlyThisEntry("/ticket/00010001/48414445.tik", "the tickets of the Internet Channel"),
        SkipEverythingInside("/title/00010001", "downloaded, Virtual Console, WiiWare and homebrew channels with their save data: not the system"),
        SkipEverythingInside("/ticket/00010001", "tickets of those channels"),

        ImportOnlyThisEntry("/shared2", "the folder of the files that all titles share"),
        ImportOnlyThisEntry("/shared2/sys", "the folder of the system files"),
        {"/shared2/sys/SYSCONF", ImportScope::OnlyThisEntry, ImportAction::ImportWithPersonalSettingsReset,
            "the system settings of the console, without the nickname, parental controls, Wii Remotes, address and clock bias of the owner"},
        SkipEverythingInside("/shared2", "state, logs and shop data of the owner's console: not clear what they hold"),

        ImportEverythingInside("/sys", "state of the title service: users, certificates, space record"),
        ImportEverythingInside("/ticket", "tickets of the system titles"),
        ImportEverythingInside("/title", "the system titles: IOS, System Menu, system channels, with their folders"),
        ImportEverythingInside("/shared1", "contents that titles share, and the map that names them"),
        ImportEverythingInside("/meta", "standard folder"),
        ImportEverythingInside("/import", "standard folder"),
        ImportEverythingInside("/tmp", "standard folder"),
        ImportEverythingInside("/wfs", "standard folder"),

        SkipEverythingInside("/", "not on the list: a path that is not clear is not imported"),
    };
}

}

std::vector<std::string> SplitPath(std::string_view path)
{
    std::vector<std::string> names;
    while (!path.empty())
    {
        if (path.front() == PathSeparator)
        {
            path.remove_prefix(1);
            continue;
        }

        std::size_t nameEnd = path.find(PathSeparator);
        if (nameEnd == std::string_view::npos)
            nameEnd = path.size();

        names.emplace_back(path.substr(0, nameEnd));
        path.remove_prefix(nameEnd);
    }
    return names;
}

std::vector<std::uint64_t> FindHomebrewChannelIds(Formats::NandTitles& titles)
{
    std::vector<std::uint64_t> homebrewChannelIds;
    for (std::uint64_t titleId : titles.ListTitleIds())
    {
        bool isIos = (titleId >> HighWordShift) == SystemTitleHighWord;
        if (isIos)
            continue;

        Formats::TitleMetadata metadata = titles.ReadMetadata(titleId);
        if (std::all_of(metadata.signature.bytes.begin(), metadata.signature.bytes.end(), IsZero))
            homebrewChannelIds.push_back(titleId);
    }
    return homebrewChannelIds;
}

std::vector<ImportRule> BuildCompleteImportRules()
{
    return {
        SkipEverythingInside("/title/00000001/00000100", "BC, the Wii side of the GameCube mode: the owner wants no GameCube"),
        SkipOnlyThisEntry("/ticket/00000001/00000100.tik", "ticket of BC"),
        SkipEverythingInside("/title/00000001/00000101", "MIOS, the GameCube mode of the console: the owner wants no GameCube"),
        SkipOnlyThisEntry("/ticket/00000001/00000101.tik", "ticket of MIOS"),
        {"/shared2/sys/net/02/config.dat", ImportScope::OnlyThisEntry, ImportAction::ImportWithWeeOSConnection,
            "the network configuration: WeeOS is online through the PC, with nothing to configure"},
        ImportEverythingInside("/", "everything else of the console, personal data included"),
    };
}

std::vector<ImportRule> BuildImportRules(const std::vector<std::uint64_t>& homebrewChannelIds)
{
    constexpr std::string_view homebrewReason = "installed with homebrew tools: its title metadata is signed with zeros";

    std::vector<ImportRule> rules;
    for (std::uint64_t titleId : homebrewChannelIds)
    {
        rules.push_back(SkipEverythingInside(Formats::TitleFolderPath(titleId), std::string(homebrewReason)));
        rules.push_back(SkipOnlyThisEntry(Formats::TicketFilePath(titleId), std::string(homebrewReason)));
    }

    std::vector<ImportRule> fixedRules = FixedImportRules();
    rules.insert(rules.end(), fixedRules.begin(), fixedRules.end());
    return rules;
}

std::size_t FindRuleNumber(const std::vector<ImportRule>& rules, std::string_view path)
{
    std::vector<std::string> pathNames = SplitPath(path);
    for (std::size_t ruleNumber = 0; ruleNumber < rules.size(); ++ruleNumber)
    {
        if (Covers(rules[ruleNumber], pathNames))
            return ruleNumber;
    }

    throw std::logic_error("The import rules do not end with a rule that covers every path");
}
