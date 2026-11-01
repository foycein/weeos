#pragma once

#include "Ios/IosFolders.h"
#include "SystemMenu/ConsoleImport.h"

#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <string>

namespace WeeOS
{

// Who picks the NAND of the Wii to import: the file picker of Windows in WeeOS.exe, a fixed file in a test.
using NandPicker = std::function<std::optional<std::filesystem::path>()>;

// Writes the console of a BootMii backup into the folders: its whole NAND with the complete import (PlanCompleteImport), and its
// identity from the keys of the backup (keys.bin beside nand.bin, or the key block at the end of nand.bin; the address of the network
// card from keys.txt when it is there). The folders must be empty or not exist.
// Throws an exception with a message for the person when the backup cannot be read or the folders cannot be written.
void ImportConsole(const std::filesystem::path& nandImagePath, const Ios::IosFolders& targetFolders);

// The import of WeeOS.exe: it runs in the background into folders beside the console's folders. When it is done, the program
// stops IOS and calls PutImportInPlace, which keeps the console that ran beside them, as "<folder> (before import)".
class ConsoleImporter : public SystemMenu::ConsoleImport
{
public:
    ConsoleImporter(Ios::IosFolders consoleFolders, NandPicker nandPicker);
    ~ConsoleImporter() override;

    // The next menu that starts offers the import (the first start of a new console).
    void OfferAtStart();

    bool TakeOfferAtStart() override;
    bool PickAndStartImport() override;
    bool IsImporting() const override;
    std::optional<std::string> TakeFailure() override;

    // True once when an import has finished well; the program then stops IOS and calls PutImportInPlace.
    bool TakeFinishedImport();

    // Moves the console that ran aside and the imported one into its folders. IOS must not run.
    void PutImportInPlace();

private:
    Ios::IosFolders folders;
    Ios::IosFolders importFolders;
    NandPicker picker;
    std::future<std::optional<std::string>> job;
    bool isImporting = false;
    bool isOfferedAtStart = false;
    std::optional<std::string> failure;
};

}
