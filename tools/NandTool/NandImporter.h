#pragma once

#include "ImportRules.h"

#include "Formats/NandFileSystem.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// What one rule has decided: how many directories and files, and how many bytes in those files.
struct RuleTotals
{
    std::uint32_t directoryCount = 0;
    std::uint32_t fileCount = 0;
    std::uint64_t byteCount = 0;
};

// The decisions of the rules for a whole NAND. Nothing is written yet.
struct ImportPlan
{
    std::vector<ImportRule> rules;

    // One for each rule, in the same order. The root directory is not counted, it is always imported.
    std::vector<RuleTotals> totals;

    // The entries that are imported, in the order of the file system: a directory comes before its entries.
    // They belong to the file system that was planned.
    std::vector<const Formats::NandEntry*> importedEntries;

    std::uint32_t importedTitleCount = 0;
    std::uint32_t skippedTitleCount = 0;
};

// Lets the rules decide for every entry. Throws a runtime_error when the rules import an entry but not its directory,
// or when an imported name cannot be stored on Windows.
ImportPlan PlanSystemImport(Formats::NandFileSystem& source);

// The same with the rules of the complete import (BuildCompleteImportRules): a person's own Wii with everything on it.
ImportPlan PlanCompleteImport(Formats::NandFileSystem& source);

// The plan of any list of rules.
ImportPlan PlanImport(Formats::NandFileSystem& source, std::vector<ImportRule> rules);

// Writes the planned entries into a folder as the NAND folder of Ios::NandStorage keeps them: one file or folder for each
// NAND entry, and the file ".attributes" with the owner, group, permissions and flags of each entry.
// A system configuration is written with its personal settings reset (see PersonalSettings.h).
// The folder must be empty or not exist yet. Nothing in it is ever deleted.
void WriteImport(Formats::NandFileSystem& source, const ImportPlan& plan, const std::filesystem::path& targetFolder);
