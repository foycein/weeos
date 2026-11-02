#include "WeeOS/ConsoleImporter.h"

#include "NandImporter.h"

#include "Formats/BootMiiKeyBlock.h"
#include "Formats/ConsoleKeys.h"
#include "Formats/FileBytes.h"
#include "Formats/NandFileSystem.h"
#include "Formats/NandImage.h"
#include "Ios/Identity/IdentityFiles.h"

#include <format>
#include <utility>

namespace WeeOS
{

namespace
{

// The files that BootMii writes beside nand.bin, and the list of keys that some dump tools add.
constexpr std::string_view KeyBlockFileName = "keys.bin";
constexpr std::string_view KeysTextFileName = "keys.txt";

constexpr std::wstring_view ImportingFolderEnding = L" (being imported)";
constexpr std::wstring_view BeforeImportFolderEnding = L" (before import)";

// keys.bin beside the backup when it is there, else the copy of it at the end of nand.bin.
Formats::Bytes ReadKeyBlock(const std::filesystem::path& nandImagePath, Formats::NandImage& image)
{
    std::filesystem::path keyBlockPath = nandImagePath.parent_path() / KeyBlockFileName;
    if (std::filesystem::exists(keyBlockPath))
        return Formats::ReadFileBytes(keyBlockPath);

    return image.ReadKeyBlock();
}

std::optional<Ios::MacAddress> ReadMacAddressBeside(const std::filesystem::path& nandImagePath)
{
    std::filesystem::path keysPath = nandImagePath.parent_path() / KeysTextFileName;
    if (!std::filesystem::exists(keysPath))
        return std::nullopt;

    return Formats::ReadConsoleKeys(Formats::ReadFileBytes(keysPath)).macAddress;
}

std::filesystem::path WithEnding(const std::filesystem::path& folder, std::wstring_view ending)
{
    std::filesystem::path changed = folder;
    changed += ending;
    return changed;
}

// "NAND (before import)", or "NAND (before import 2)" and so on when that one is there already.
std::filesystem::path FreeFolderBeforeImport(const std::filesystem::path& folder)
{
    std::filesystem::path candidate = WithEnding(folder, BeforeImportFolderEnding);
    for (int number = 2; std::filesystem::exists(candidate); ++number)
        candidate = WithEnding(folder, std::format(L" (before import {})", number));

    return candidate;
}

void MoveAsideAndReplace(const std::filesystem::path& folder, const std::filesystem::path& replacement)
{
    if (std::filesystem::exists(folder))
        std::filesystem::rename(folder, FreeFolderBeforeImport(folder));

    std::filesystem::rename(replacement, folder);
}

// The import of the background: nothing when it went well, else why not.
std::optional<std::string> RunImport(const std::filesystem::path& nandImagePath, const Ios::IosFolders& targetFolders)
{
    try
    {
        std::filesystem::remove_all(targetFolders.nandFolder);
        std::filesystem::remove_all(targetFolders.identityFolder);
        ImportConsole(nandImagePath, targetFolders);
        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        return std::format("The Wii could not be imported: {}", error.what());
    }
}

}

void ImportConsole(const std::filesystem::path& nandImagePath, const Ios::IosFolders& targetFolders)
{
    Formats::NandImage image(nandImagePath);
    Formats::Bytes keyBlock = ReadKeyBlock(nandImagePath, image);
    Formats::BootMiiKeyBlock keys = Formats::ReadBootMiiKeyBlock(keyBlock);

    Formats::NandFileSystem fileSystem(image, keys.oneTimeProgrammable.nandAesKey);
    Ios::SharedKeys sharedKeys = Ios::FindSharedKeysInNand(fileSystem);
    WriteImport(fileSystem, PlanCompleteImport(fileSystem), targetFolders.nandFolder);
    Ios::WriteIdentityFromKeyBlock(keyBlock, ReadMacAddressBeside(nandImagePath), sharedKeys, targetFolders.identityFolder);
}

ConsoleImporter::ConsoleImporter(Ios::IosFolders consoleFolders, NandPicker nandPicker)
    : folders(std::move(consoleFolders))
    , picker(std::move(nandPicker))
{
    importFolders = folders;
    importFolders.nandFolder = WithEnding(folders.nandFolder, ImportingFolderEnding);
    importFolders.identityFolder = WithEnding(folders.identityFolder, ImportingFolderEnding);
}

ConsoleImporter::~ConsoleImporter()
{
    if (job.valid())
        job.wait();
}

void ConsoleImporter::OfferAtStart()
{
    isOfferedAtStart = true;
}

bool ConsoleImporter::TakeOfferAtStart()
{
    return std::exchange(isOfferedAtStart, false);
}

bool ConsoleImporter::PickAndStartImport()
{
    if (isImporting)
        return false;

    std::optional<std::filesystem::path> nandImagePath = picker();
    if (!nandImagePath)
        return false;

    isImporting = true;
    job = std::async(std::launch::async, RunImport, *nandImagePath, importFolders);
    return true;
}

bool ConsoleImporter::IsImporting() const
{
    return isImporting;
}

std::optional<std::string> ConsoleImporter::TakeFailure()
{
    return std::exchange(failure, std::nullopt);
}

bool ConsoleImporter::TakeFinishedImport()
{
    if (!job.valid())
        return false;

    bool isDone = job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    if (!isDone)
        return false;

    failure = job.get();
    if (failure)
    {
        isImporting = false;
        return false;
    }

    return true;
}

void ConsoleImporter::PutImportInPlace()
{
    MoveAsideAndReplace(folders.nandFolder, importFolders.nandFolder);
    MoveAsideAndReplace(folders.identityFolder, importFolders.identityFolder);
    isImporting = false;
}

}
