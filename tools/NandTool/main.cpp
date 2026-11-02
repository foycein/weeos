#include "DumpVerification.h"
#include "FolderFiles.h"
#include "IdentityImporter.h"
#include "NandImporter.h"

#include "Formats/ConsoleKeys.h"
#include "Formats/FileBytes.h"
#include "Formats/NandFileSystem.h"
#include "Formats/NandImage.h"
#include "Formats/NandTitles.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr std::string_view UsageText =
    "Usage: NandTool --dump <folder> <command> [arguments]\n"
    "\n"
    "  list [path]                 the entries of a directory (default: /)\n"
    "  tree [path]                 everything below a path (default: /)\n"
    "  extract <path> <folder>     a file, or a directory with everything in it, decrypted, into <folder>\n"
    "  titles                      every title with its version, number of contents and required IOS\n"
    "  title <id> <folder>         title.tmd, ticket.tik and all contents of a title into <folder>, like 00000001-00000002\n"
    "  import <folder>             the system part of the NAND into <folder>, as the NAND folder of the IOS part keeps it;\n"
    "                              SYSCONF comes without the nickname, parental controls, Wii Remotes, address and clock bias;\n"
    "                              BC and MIOS (the GameCube) are left out\n"
    "  identity <folder>           keys.txt, device.cert and seeprom.bin into <folder>, the identity folder of the IOS part\n"
    "  verify                     the error correction of all pages, the HMAC of all clusters, the contents and the signatures of all titles\n"
    "\n"
    "The dump is the folder of a console's backup, with nand.bin and keys.txt.\n"
    "The NAND keeps the contents of titles already decrypted, so title and extract write them as they are stored;\n"
    "title checks every content against the SHA-1 hash in the title metadata first.\n"
    "Only the folders named in a command are written to, and import refuses a folder that is not empty.\n";

constexpr std::string_view DumpOption = "--dump";
constexpr std::string_view NandImageFileName = "nand.bin";
constexpr std::string_view KeysFileName = "keys.txt";
constexpr std::string_view RootPath = "/";

constexpr std::uint32_t FirstPrintableCharacter = 0x20;
constexpr std::uint32_t LastPrintableCharacter = 0x7E;

// The low half of a title id is often four letters, like 48414241 ("HABA").
constexpr int TitleCodeLength = 4;

// The option --dump and the folder after it.
constexpr std::size_t DumpOptionWordCount = 2;

constexpr std::uint32_t EveryPage = 1;

class UsageError : public std::runtime_error
{
public:
    UsageError()
        : std::runtime_error("wrong arguments")
    {
    }
};

// The files of the dump that the commands need. The file system reads from the image, so they stay together.
class Dump
{
public:
    explicit Dump(const std::filesystem::path& dumpFolder)
        : folder(dumpFolder)
        , keys(Formats::ReadConsoleKeys(Formats::ReadFileBytes(dumpFolder / KeysFileName)))
        , image(dumpFolder / NandImageFileName)
        , fileSystem(image, keys.nandAesKey)
    {
    }

    std::filesystem::path folder;
    Formats::ConsoleKeys keys;
    Formats::NandImage image;
    Formats::NandFileSystem fileSystem;
};

using Arguments = std::vector<std::string>;

// Accepts "title/00000001/" as well as "/title/00000001".
std::string NormalizePath(std::string path)
{
    while (path.size() > 1 && path.ends_with('/'))
        path.pop_back();

    if (!path.starts_with('/'))
        path.insert(path.begin(), '/');

    return path;
}

// The path a command works on: its one optional argument, or the root when there is none.
std::string PathArgumentOrRoot(const Arguments& arguments)
{
    if (arguments.size() > 1)
        throw UsageError();

    if (arguments.empty())
        return std::string(RootPath);

    return NormalizePath(arguments[0]);
}

const Formats::NandEntry& FindEntryOrFail(const Formats::NandFileSystem& fileSystem, const std::string& path)
{
    const Formats::NandEntry* entry = fileSystem.Find(path);
    if (entry == nullptr)
        throw std::runtime_error(std::format("{} does not exist on the NAND", path));

    return *entry;
}

void PrintEntry(const Formats::NandEntry& entry, const std::string& shownName)
{
    char kindLetter = entry.kind == Formats::NandEntryKind::Directory ? 'd' : '-';
    std::cout << std::format("{} {} {} {}  {:08x} {:04x} {:>10}  {}\n", kindLetter,
        Formats::RightsText(entry.ownerRights), Formats::RightsText(entry.groupRights), Formats::RightsText(entry.otherRights),
        entry.userId, entry.groupId, entry.size, shownName);
}

// True for the path itself and for everything below it.
bool IsInside(const std::string& path, const std::string& directoryPath)
{
    if (directoryPath == RootPath)
        return true;

    return path == directoryPath || path.starts_with(directoryPath + "/");
}

void ListCommand(Dump& dump, const Arguments& arguments)
{
    std::string path = PathArgumentOrRoot(arguments);
    const Formats::NandEntry& entry = FindEntryOrFail(dump.fileSystem, path);
    if (entry.kind == Formats::NandEntryKind::File)
    {
        PrintEntry(entry, entry.name);
        return;
    }

    for (const Formats::NandEntry* child : dump.fileSystem.ListDirectory(path))
        PrintEntry(*child, child->name);
}

void TreeCommand(Dump& dump, const Arguments& arguments)
{
    std::string path = PathArgumentOrRoot(arguments);
    FindEntryOrFail(dump.fileSystem, path);

    for (const Formats::NandEntry& entry : dump.fileSystem.Entries())
    {
        if (entry.path != RootPath && IsInside(entry.path, path))
            PrintEntry(entry, entry.path);
    }
}

struct ExtractTotals
{
    std::uint32_t fileCount = 0;
    std::uint64_t byteCount = 0;
};

void ExtractEntry(Formats::NandFileSystem& fileSystem, const Formats::NandEntry& entry, const std::filesystem::path& target,
    ExtractTotals& totals)
{
    if (entry.kind == Formats::NandEntryKind::File)
    {
        Formats::Bytes content = fileSystem.ReadFile(entry);
        WriteBytesToFile(target, content);
        ++totals.fileCount;
        totals.byteCount += content.size();
        return;
    }

    std::filesystem::create_directories(target);
    for (const Formats::NandEntry* child : fileSystem.ListDirectory(entry.path))
        ExtractEntry(fileSystem, *child, target / child->name, totals);
}

void ExtractCommand(Dump& dump, const Arguments& arguments)
{
    if (arguments.size() != 2)
        throw UsageError();

    const Formats::NandEntry& entry = FindEntryOrFail(dump.fileSystem, NormalizePath(arguments[0]));
    std::filesystem::path folder = arguments[1];

    // The root has no name, so its entries go straight into the folder.
    std::filesystem::path target = entry.path == RootPath ? folder : folder / entry.name;
    std::filesystem::create_directories(folder);

    ExtractTotals totals;
    ExtractEntry(dump.fileSystem, entry, target, totals);
    std::cout << std::format("Extracted {} files, {} bytes, into {}\n", totals.fileCount, totals.byteCount, target.string());
}

// The four letters of a title id like 00010002-48414241 ("HABA"), or nothing when the low half is no text.
std::string TitleCodeOf(std::uint64_t titleId)
{
    std::string code;
    for (int byteNumber = TitleCodeLength - 1; byteNumber >= 0; --byteNumber)
    {
        std::uint32_t character = (titleId >> (byteNumber * Formats::BitsPerByte)) & Formats::ByteMask;
        bool isPrintable = character >= FirstPrintableCharacter && character <= LastPrintableCharacter;
        if (!isPrintable)
            return std::string();

        code += static_cast<char>(character);
    }
    return code;
}

// The IOS a title runs on is the low half of the required system version: IOS80 is 00000001-00000050.
std::string RequiredIosText(const Formats::TitleMetadata& metadata)
{
    std::uint32_t iosNumber = static_cast<std::uint32_t>(metadata.requiredSystemVersion);
    if (iosNumber == 0)
        return "-";

    return std::format("IOS{}", iosNumber);
}

void TitlesCommand(Dump& dump, const Arguments& arguments)
{
    if (!arguments.empty())
        throw UsageError();

    Formats::NandTitles titles(dump.fileSystem);
    std::vector<std::uint64_t> titleIds = titles.ListTitleIds();
    for (std::uint64_t titleId : titleIds)
    {
        Formats::TitleMetadata metadata = titles.ReadMetadata(titleId);
        std::cout << std::format("{}  {:<4}  version {:<6}  {:>2} contents  requires {}\n", Formats::FormatTitleId(titleId),
            TitleCodeOf(titleId), metadata.titleVersion, metadata.contents.size(), RequiredIosText(metadata));
    }
    std::cout << std::format("{} titles\n", titleIds.size());
}

void TitleCommand(Dump& dump, const Arguments& arguments)
{
    if (arguments.size() != 2)
        throw UsageError();

    std::uint64_t titleId = Formats::ParseTitleId(arguments[0]);
    std::filesystem::path folder = arguments[1];

    Formats::NandTitles titles(dump.fileSystem);
    Formats::Bytes metadataBytes = titles.ReadMetadataBytes(titleId);
    Formats::TitleMetadata metadata = Formats::ReadTitleMetadata(metadataBytes);

    std::filesystem::create_directories(folder);
    WriteBytesToFile(folder / "title.tmd", metadataBytes);

    std::optional<Formats::Bytes> ticketBytes = titles.ReadTicketBytes(titleId);
    if (ticketBytes)
        WriteBytesToFile(folder / "ticket.tik", *ticketBytes);

    std::uint32_t missingCount = 0;
    for (const Formats::ContentRecord& record : metadata.contents)
    {
        if (!titles.FindContentPath(titleId, record))
        {
            std::cout << std::format("{:08x}.app  is not on the NAND\n", record.id);
            ++missingCount;
            continue;
        }

        Formats::Bytes content = titles.ReadContent(titleId, record);
        WriteBytesToFile(folder / std::format("{:08x}.app", record.id), content);
        std::cout << std::format("{:08x}.app  {:>10} bytes  index {}  SHA-1 matches\n", record.id, content.size(), record.index);
    }

    if (missingCount > 0)
        throw std::runtime_error(std::format("{} contents of the title are not on the NAND", missingCount));
}

// The totals of the rules that import (any kind of import) or of the rules that skip.
RuleTotals AddUpRules(const ImportPlan& plan, ImportAction action)
{
    RuleTotals sum;
    for (std::size_t ruleNumber = 0; ruleNumber < plan.rules.size(); ++ruleNumber)
    {
        if (IsImported(plan.rules[ruleNumber].action) != IsImported(action))
            continue;

        sum.directoryCount += plan.totals[ruleNumber].directoryCount;
        sum.fileCount += plan.totals[ruleNumber].fileCount;
        sum.byteCount += plan.totals[ruleNumber].byteCount;
    }
    return sum;
}

std::string_view ActionText(ImportAction action)
{
    if (action == ImportAction::ImportWithPersonalSettingsReset)
        return "cleaned";

    return IsImported(action) ? "import" : "skip";
}

void PrintImportReport(const ImportPlan& plan)
{
    std::cout << " rule  action   directories   files        bytes  pattern: reason\n";
    for (std::size_t ruleNumber = 0; ruleNumber < plan.rules.size(); ++ruleNumber)
    {
        const ImportRule& rule = plan.rules[ruleNumber];
        const RuleTotals& totals = plan.totals[ruleNumber];
        std::string_view actionText = ActionText(rule.action);
        std::string_view scopeText = rule.scope == ImportScope::OnlyThisEntry ? "" : " and everything inside";
        std::cout << std::format("{:>5}  {:<7}  {:>11}  {:>6}  {:>11}  {}{}: {}\n", ruleNumber + 1, actionText, totals.directoryCount,
            totals.fileCount, totals.byteCount, rule.pattern, scopeText, rule.reason);
    }

    RuleTotals imported = AddUpRules(plan, ImportAction::Import);
    RuleTotals skipped = AddUpRules(plan, ImportAction::Skip);
    std::cout << std::format("Imported: {} titles, {} directories, {} files, {} bytes\n", plan.importedTitleCount,
        imported.directoryCount, imported.fileCount, imported.byteCount);
    std::cout << std::format("Skipped:  {} titles, {} directories, {} files, {} bytes\n", plan.skippedTitleCount,
        skipped.directoryCount, skipped.fileCount, skipped.byteCount);
}

void ImportCommand(Dump& dump, const Arguments& arguments)
{
    if (arguments.size() != 1)
        throw UsageError();

    ImportPlan plan = PlanSystemImport(dump.fileSystem);
    WriteImport(dump.fileSystem, plan, arguments[0]);
    PrintImportReport(plan);
}

std::string_view CheckText(Formats::SignatureCheck check)
{
    switch (check)
    {
    case Formats::SignatureCheck::Valid:
        return "valid";
    case Formats::SignatureCheck::SignerNotFound:
        return "signer not found";
    case Formats::SignatureCheck::Invalid:
        return "invalid";
    }
    return "unknown";
}

void PrintErrorCorrectionReport(Formats::NandImage& image)
{
    ErrorCorrectionReport report = CheckErrorCorrection(image, EveryPage);
    std::cout << std::format("Error correction of the pages: {} erased, {} correct, {} without error correction bytes, {} wrong\n",
        report.erasedPageCount, report.correctPageCount, report.unprotectedPages.size(), report.wrongPages.size());

    for (std::uint32_t pageNumber : report.unprotectedPages)
        std::cout << std::format("  page {} (cluster {}, block {}) has data and no error correction bytes\n", pageNumber,
            pageNumber / Formats::NandPagesPerCluster, pageNumber / Formats::NandPagesPerBlock);

    for (std::uint32_t pageNumber : report.wrongPages)
        std::cout << std::format("  page {} (cluster {}, block {}) has wrong error correction\n", pageNumber,
            pageNumber / Formats::NandPagesPerCluster, pageNumber / Formats::NandPagesPerBlock);
}

void PrintHmacReport(Dump& dump)
{
    HmacReport report = CheckHmacs(dump.fileSystem, dump.keys.nandHmacKey, std::numeric_limits<std::uint32_t>::max());
    std::cout << std::format("HMAC of the clusters: {} files with {} clusters, {} wrong; the superblock {}\n", report.checkedFileCount,
        report.checkedClusterCount, report.wrongClusterCount, report.superblockIsCorrect ? "is right" : "is wrong");
}

void PrintContentReport(Formats::NandFileSystem& fileSystem)
{
    std::vector<std::uint64_t> titleIds = Formats::NandTitles(fileSystem).ListTitleIds();
    ContentReport report = CheckContentsOfTitles(fileSystem, titleIds);
    std::cout << std::format("Contents of the titles against their title metadata: {} checked, {} missing, {} wrong\n", report.checkedCount,
        report.missingCount, report.wrongCount);
}

void PrintSignatureReport(Formats::NandFileSystem& fileSystem)
{
    std::vector<std::uint64_t> titleIds = Formats::NandTitles(fileSystem).ListTitleIds();
    std::vector<TitleSignatureReport> reports = CheckSignaturesOfTitles(fileSystem, titleIds);

    std::size_t titleCountWithValidMetadata = 0;
    std::size_t ticketCount = 0;
    std::size_t ticketCountThatIsValid = 0;
    for (const TitleSignatureReport& report : reports)
    {
        if (report.metadata == Formats::SignatureCheck::Valid)
            ++titleCountWithValidMetadata;
        else
            std::cout << std::format("  {} title metadata: {}\n", Formats::FormatTitleId(report.titleId), CheckText(report.metadata));

        for (Formats::SignatureCheck ticketCheck : report.tickets)
        {
            ++ticketCount;
            if (ticketCheck == Formats::SignatureCheck::Valid)
                ++ticketCountThatIsValid;
            else
                std::cout << std::format("  {} ticket: {}\n", Formats::FormatTitleId(report.titleId), CheckText(ticketCheck));
        }
    }

    std::cout << std::format("Signatures: {} of {} title metadata files and {} of {} tickets are valid\n", titleCountWithValidMetadata,
        reports.size(), ticketCountThatIsValid, ticketCount);
}

void IdentityCommand(Dump& dump, const Arguments& arguments)
{
    if (arguments.size() != 1)
        throw UsageError();

    WriteIdentity(dump.folder, arguments[0]);
    std::cout << std::format("The identity of the console {:08X} is in {}\n", dump.keys.consoleId, arguments[0]);
}

void VerifyCommand(Dump& dump, const Arguments& arguments)
{
    if (!arguments.empty())
        throw UsageError();

    PrintErrorCorrectionReport(dump.image);
    PrintHmacReport(dump);
    PrintContentReport(dump.fileSystem);
    PrintSignatureReport(dump.fileSystem);
}

struct Command
{
    std::string_view name;
    void (*run)(Dump& dump, const Arguments& arguments);
};

constexpr Command Commands[] =
{
    {"list", ListCommand},
    {"tree", TreeCommand},
    {"extract", ExtractCommand},
    {"titles", TitlesCommand},
    {"title", TitleCommand},
    {"import", ImportCommand},
    {"identity", IdentityCommand},
    {"verify", VerifyCommand},
};

const Command& FindCommand(const std::string& name)
{
    for (const Command& command : Commands)
    {
        if (command.name == name)
            return command;
    }
    throw UsageError();
}

int Run(Arguments arguments)
{
    if (arguments.size() < DumpOptionWordCount || arguments[0] != DumpOption)
        throw UsageError();

    std::filesystem::path dumpFolder = arguments[1];
    arguments.erase(arguments.begin(), arguments.begin() + DumpOptionWordCount);

    if (arguments.empty())
        throw UsageError();

    const Command& command = FindCommand(arguments.front());
    arguments.erase(arguments.begin());

    Dump dump(dumpFolder);
    command.run(dump, arguments);
    return 0;
}

}

int main(int argumentCount, char** argumentValues)
{
    Arguments arguments(argumentValues + 1, argumentValues + argumentCount);

    try
    {
        return Run(arguments);
    }
    catch (const UsageError&)
    {
        std::cout << UsageText;
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cout << "Error: " << error.what() << "\n";
        return 1;
    }
}
