#pragma once

#include "Formats/NandTitles.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum class ImportAction
{
    Import,

    // The entry is a system configuration (SYSCONF). It is imported with the settings of the owner set back to the factory values.
    ImportWithPersonalSettingsReset,

    // The entry is the network configuration (config.dat). The owner's networks stay out; WeeOS's one connection takes their place.
    ImportWithWeeOSConnection,

    Skip,
};

inline bool IsImported(ImportAction action)
{
    return action != ImportAction::Skip;
}

// A rule covers either only the entry at its pattern, or that entry and everything inside it.
enum class ImportScope
{
    OnlyThisEntry,
    EverythingInside,
};

struct ImportRule
{
    // A path like "/shared2/wc24". A star stands for any one name: "/title/*/*/data" is the data folder of every title.
    std::string pattern;
    ImportScope scope;
    ImportAction action;
    std::string reason;
};

// "/title/00000001" becomes {"title", "00000001"}. The root becomes an empty list.
std::vector<std::string> SplitPath(std::string_view path);

// The channels that homebrew tools installed: their title metadata is signed with zeros, which no real signature is.
// IOS are not looked at. A patched IOS is still a part of the system.
std::vector<std::uint64_t> FindHomebrewChannelIds(Formats::NandTitles& titles);

// The rules in the order they are tried: first the rules for the homebrew channels, then the fixed rules of the owner.
// The last rule covers every path, so every path follows a rule, and a path that no other rule names is not imported.
std::vector<ImportRule> BuildImportRules(const std::vector<std::uint64_t>& homebrewChannelIds);

// The rules of the complete import, which brings a person's own Wii into WeeOS (docs/DECISIONS.md, Start): everything of that console,
// its Miis, saves, channels and Message Board included. Only what WeeOS has no place for stays out: BC and MIOS (no GameCube), and the
// networks of the console, whose place WeeOS's connection through the PC takes (config.dat).
std::vector<ImportRule> BuildCompleteImportRules();

// The number of the first rule that covers the path. Throws a logic_error when the rules do not end with a rule for every path.
std::size_t FindRuleNumber(const std::vector<ImportRule>& rules, std::string_view path);
