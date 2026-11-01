#pragma once

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Power/PowerService.h"
#include "Ios/Title/TitleService.h"

namespace Settings
{

// "Format Wii System Memory" of Wii Settings: the console erases what the user added to it. The channels that were added and the data
// of every title are gone, the Message Board and its data, and every file of the shared folder except the settings of the console.
// What the console needs to start stays: boot2, the System Menu, every IOS, the system channels and their tickets, the shared contents,
// the settings of the console (SYSCONF) and the product information file of the System Menu.
//
// This is a choice of WeeOS (docs/audit/OpenPoints/Settings.md SET-07): the original asks the NAND library for the format and the owner has no
// console to look at what it keeps. The settings stay because WeeOS has no first-time setup to bring them back.
class SystemMemoryFormatter
{
public:
    // The services must outlive the formatter.
    SystemMemoryFormatter(Ios::FileSystemService& fileSystem, Ios::TitleService& titles, Ios::PowerService& power);

    // Erases the data. Throws a SettingsError when the file system refuses.
    void Format();

    // The console switches off, as the page says after the format.
    void ShutDownConsole();

private:
    void DeleteTitlesOfTheUser();
    void DeleteTicketsOfTheUser();
    void EmptyDataFolderOfSystemChannels();
    void EmptyDataFolderOfTheSystemMenu();
    void EmptySharedFolder();

    // Deletes everything inside the directory except the entries with the names in the list.
    void EmptyDirectory(const Ios::NandPath& directory, const std::vector<std::string>& namesToKeep);
    void DeleteEntry(const Ios::NandPath& path);

    Ios::FileSystemService& fileSystem;
    Ios::TitleService& titles;
    Ios::PowerService& power;
};

}
