#include "Installation.h"

#include "Package.h"

#include "Formats/FileBytes.h"
#include "Formats/ZipArchive.h"
#include "SystemMenu/WeeOSVersion.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <format>
#include <set>
#include <stdexcept>
#include <vector>

namespace Setup
{

namespace
{

using Microsoft::WRL::ComPtr;

constexpr wchar_t ProgramFolderName[] = L"WeeOS";
constexpr wchar_t ProgramFileName[] = L"WeeOS.exe";
constexpr wchar_t ShortcutFileName[] = L"WeeOS.lnk";
constexpr wchar_t ProgramDescription[] = L"A hand-written reconstruction of the Wii system for the PC";
constexpr wchar_t Publisher[] = L"foycein";

constexpr wchar_t UninstallKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WeeOS";

// Every name in the zip starts with the folder WeeOS, which the portable zip unpacks to. The setup puts what is inside it into the
// chosen folder.
constexpr std::string_view PackageFolderPrefix = "WeeOS/";

constexpr std::uint64_t BytesPerKilobyte = 1024;

const std::string DamagedPackageMessage = "The package inside the setup is damaged. Please download it again.";

// The console of the person lies beside WeeOS.exe (source/WeeOS/main.cpp): the NAND and the identity, also the ones an import kept
// ("NAND (before import)"), the SD card and the settings of the PC.
bool IsPartOfTheConsole(const std::wstring& name)
{
    return name.starts_with(L"NAND") || name.starts_with(L"Identity") || name == L"SDCard" || name == L"WeeOS.settings.txt";
}

std::filesystem::path RelativePathInPackage(const Formats::ZipEntry& entry)
{
    if (!entry.name.starts_with(PackageFolderPrefix))
        throw std::runtime_error(DamagedPackageMessage);

    std::filesystem::path relativePath(entry.name.substr(PackageFolderPrefix.size()));
    if (relativePath.has_root_path())
        throw std::runtime_error(DamagedPackageMessage);

    for (const std::filesystem::path& part : relativePath)
    {
        if (part == L"..")
            throw std::runtime_error(DamagedPackageMessage);
    }
    return relativePath;
}

std::uint64_t UnpackedSizeOf(const std::vector<Formats::ZipEntry>& entries)
{
    std::uint64_t size = 0;
    for (const Formats::ZipEntry& entry : entries)
        size += entry.unpackedSize;

    return size;
}

void UnpackEntries(const std::vector<Formats::ZipEntry>& entries, const std::filesystem::path& folder, const ProgressReport& reportProgress)
{
    std::uint64_t totalSize = UnpackedSizeOf(entries);
    std::uint64_t doneSize = 0;
    for (const Formats::ZipEntry& entry : entries)
    {
        std::filesystem::path target = folder / RelativePathInPackage(entry);
        if (Formats::IsFolder(entry))
        {
            std::filesystem::create_directories(target);
            continue;
        }

        std::filesystem::create_directories(target.parent_path());
        Formats::WriteFileBytes(target, Formats::UnpackZipEntry(entry));

        doneSize += entry.unpackedSize;
        reportProgress(static_cast<double>(doneSize) / static_cast<double>(totalSize));
    }
}

// What the uninstaller removes: the first name of every path in the package, apart from the console's own folders.
std::vector<std::wstring> ProgramNamesOf(const std::vector<Formats::ZipEntry>& entries)
{
    std::set<std::wstring> names;
    for (const Formats::ZipEntry& entry : entries)
    {
        std::filesystem::path relativePath = RelativePathInPackage(entry);
        if (relativePath.empty())
            continue;

        std::wstring firstName = relativePath.begin()->wstring();
        if (!IsPartOfTheConsole(firstName))
            names.insert(firstName);
    }
    return std::vector<std::wstring>(names.begin(), names.end());
}

std::filesystem::path KnownFolder(const KNOWNFOLDERID& folderId)
{
    PWSTR folderText = nullptr;
    HRESULT result = SHGetKnownFolderPath(folderId, KF_FLAG_CREATE, nullptr, &folderText);
    std::filesystem::path folder = SUCCEEDED(result) ? folderText : L"";
    CoTaskMemFree(folderText);

    if (folder.empty())
        throw std::runtime_error("A folder of Windows (the desktop, the Start menu or the programs) could not be found.");

    return folder;
}

std::vector<std::filesystem::path> ShortcutFiles()
{
    return {KnownFolder(FOLDERID_Desktop) / ShortcutFileName, KnownFolder(FOLDERID_Programs) / ShortcutFileName};
}

void ThrowWhenFailed(HRESULT result, const char* message)
{
    if (FAILED(result))
        throw std::runtime_error(message);
}

void MakeShortcut(const std::filesystem::path& shortcutFile, const std::filesystem::path& programFile)
{
    const char* message = "A shortcut to WeeOS could not be made.";

    ComPtr<IShellLinkW> shortcut;
    ThrowWhenFailed(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shortcut)), message);
    ThrowWhenFailed(shortcut->SetPath(programFile.c_str()), message);
    ThrowWhenFailed(shortcut->SetWorkingDirectory(programFile.parent_path().c_str()), message);
    ThrowWhenFailed(shortcut->SetDescription(ProgramDescription), message);

    ComPtr<IPersistFile> file;
    ThrowWhenFailed(shortcut.As(&file), message);
    BOOL rememberTheFile = TRUE;
    ThrowWhenFailed(file->Save(shortcutFile.c_str(), rememberTheFile), message);
}

class RegistryKey
{
public:
    explicit RegistryKey(const wchar_t* path)
    {
        LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
        if (status != ERROR_SUCCESS)
            throw std::runtime_error("WeeOS could not be added to the apps of Windows.");
    }

    ~RegistryKey()
    {
        RegCloseKey(key);
    }

    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;

    void WriteText(const wchar_t* name, const std::wstring& text)
    {
        DWORD sizeWithEnd = static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t));
        Check(RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(text.c_str()), sizeWithEnd));
    }

    void WriteNumber(const wchar_t* name, DWORD number)
    {
        Check(RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&number), sizeof(number)));
    }

private:
    static void Check(LSTATUS status)
    {
        if (status != ERROR_SUCCESS)
            throw std::runtime_error("WeeOS could not be added to the apps of Windows.");
    }

    HKEY key = nullptr;
};

// The entry in "Installed apps" of Windows, which starts the uninstaller.
void WriteUninstallEntry(const std::filesystem::path& folder, std::uint64_t installedSize)
{
    std::wstring uninstaller = L"\"" + (folder / UninstallerFileName).wstring() + L"\"";

    RegistryKey key(UninstallKeyPath);
    key.WriteText(L"DisplayName", ProgramFolderName);
    key.WriteText(L"DisplayVersion", WeeOSVersionText());
    key.WriteText(L"Publisher", Publisher);
    key.WriteText(L"DisplayIcon", (folder / ProgramFileName).wstring());
    key.WriteText(L"InstallLocation", folder.wstring());
    key.WriteText(L"UninstallString", uninstaller);
    key.WriteText(L"QuietUninstallString", uninstaller + L" " + SilentOption);
    key.WriteNumber(L"NoModify", 1);
    key.WriteNumber(L"NoRepair", 1);
    key.WriteNumber(L"EstimatedSize", static_cast<DWORD>(installedSize / BytesPerKilobyte));
}

std::filesystem::path SystemFolder()
{
    std::wstring folder(MAX_PATH, L'\0');
    UINT length = GetSystemDirectoryW(folder.data(), static_cast<UINT>(folder.size()));
    folder.resize(length);
    return folder;
}

// A running program cannot remove its own file. A hidden command window waits two seconds, removes the uninstaller and then the
// folder, which goes only when it is empty: a console that the person kept stays.
void RemoveUninstallerLater(const std::filesystem::path& folder)
{
    std::filesystem::path commandProgram = SystemFolder() / L"cmd.exe";
    std::wstring command = std::format(L"\"{}\" /c ping -n 3 127.0.0.1 >nul & del /f /q \"{}\" & rmdir \"{}\"",
        commandProgram.wstring(), (folder / UninstallerFileName).wstring(), folder.wstring());

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    BOOL inheritHandles = FALSE;
    std::filesystem::path workingFolder = SystemFolder();
    BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, inheritHandles, CREATE_NO_WINDOW, nullptr,
        workingFolder.c_str(), &startup, &process);
    if (!started)
        return;

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

void RemoveConsole(const std::filesystem::path& folder)
{
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder))
    {
        if (IsPartOfTheConsole(entry.path().filename().wstring()))
            std::filesystem::remove_all(entry.path());
    }
}

}

ComApartment::ComApartment()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

ComApartment::~ComApartment()
{
    CoUninitialize();
}

std::wstring WeeOSVersionText()
{
    return std::wstring(SystemMenu::WeeOSVersion.begin(), SystemMenu::WeeOSVersion.end());
}

std::filesystem::path DefaultInstallFolder()
{
    return KnownFolder(FOLDERID_UserProgramFiles) / ProgramFolderName;
}

std::filesystem::path InstallFolderInside(const std::filesystem::path& chosenFolder)
{
    if (_wcsicmp(chosenFolder.filename().c_str(), ProgramFolderName) == 0)
        return chosenFolder;

    return chosenFolder / ProgramFolderName;
}

void InstallWeeOS(const std::filesystem::path& folder, const ProgressReport& reportProgress)
{
    ComApartment comForTheShortcuts;

    std::vector<Formats::ZipEntry> entries = Formats::ReadZipEntries(CarriedPackage());
    std::filesystem::create_directories(folder);
    UnpackEntries(entries, folder, reportProgress);
    WriteUninstaller(folder / UninstallerFileName, ProgramNamesOf(entries));

    std::filesystem::path programFile = folder / ProgramFileName;
    for (const std::filesystem::path& shortcutFile : ShortcutFiles())
        MakeShortcut(shortcutFile, programFile);

    WriteUninstallEntry(folder, UnpackedSizeOf(entries));
}

std::filesystem::path InstalledFolder()
{
    return ThisProgramFile().parent_path();
}

void UninstallWeeOS(bool removeConsole)
{
    std::vector<std::wstring> programNames = CarriedProgramNames();
    if (programNames.empty())
        throw std::runtime_error("This program is not the uninstaller of an installed WeeOS.");

    std::filesystem::path folder = InstalledFolder();
    for (const std::wstring& name : programNames)
        std::filesystem::remove_all(folder / name);

    if (removeConsole)
        RemoveConsole(folder);

    for (const std::filesystem::path& shortcutFile : ShortcutFiles())
    {
        std::error_code ignoredError;
        std::filesystem::remove(shortcutFile, ignoredError);
    }

    RegDeleteTreeW(HKEY_CURRENT_USER, UninstallKeyPath);
    RemoveUninstallerLater(folder);
}

void StartWeeOS(const std::filesystem::path& folder)
{
    std::filesystem::path programFile = folder / ProgramFileName;
    ShellExecuteW(nullptr, L"open", programFile.c_str(), nullptr, folder.c_str(), SW_SHOWNORMAL);
}

}
