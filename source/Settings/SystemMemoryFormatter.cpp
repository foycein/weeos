#include "Settings/SystemMemoryFormatter.h"

#include "Settings/SettingsError.h"

#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <format>

namespace Settings
{

namespace
{

// The titles that a user can add: games and downloaded channels. The system channels (00010002 and 00010008) stay.
constexpr std::uint32_t GameHighPart = 0x00010000;
constexpr std::uint32_t DownloadedChannelHighPart = 0x00010001;
constexpr std::uint32_t WiiWareHighPart = 0x00010004;
constexpr std::uint32_t DownloadedContentHighPart = 0x00010005;
constexpr std::uint32_t SystemChannelHighPart = 0x00010002;
constexpr std::uint32_t HiddenSystemChannelHighPart = 0x00010008;

// The System Menu keeps the product information of the console in its data folder. The console never changes it.
constexpr std::string_view ProductInformationFileName = "setting.txt";

constexpr std::string_view SharedFolderPath = "/shared2";
constexpr std::string_view SystemSharedFolderName = "sys";
constexpr std::string_view SettingsFileName = "SYSCONF";

bool IsTitleOfTheUser(const Ios::TitleId& titleId)
{
    return titleId.highPart == GameHighPart || titleId.highPart == DownloadedChannelHighPart || titleId.highPart == WiiWareHighPart
        || titleId.highPart == DownloadedContentHighPart;
}

bool IsSystemChannel(const Ios::TitleId& titleId)
{
    return titleId.highPart == SystemChannelHighPart || titleId.highPart == HiddenSystemChannelHighPart;
}

Ios::NandPath ParsePath(std::string_view text)
{
    Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(text);
    if (path.Failed())
        throw SettingsError(std::format("The path {} is not accepted (result code {})", text, Ios::OriginalResultCode(path.Error())));

    return path.Value();
}

Ios::NandPath ChildPath(const Ios::NandPath& directory, const std::string& name)
{
    Ios::Result<Ios::NandPath> child = directory.Child(name);
    if (child.Failed())
        throw SettingsError(std::format("The name {} is not accepted in {} (result code {})", name, directory.ToString(),
            Ios::OriginalResultCode(child.Error())));

    return child.Value();
}

}

SystemMemoryFormatter::SystemMemoryFormatter(Ios::FileSystemService& fileSystem, Ios::TitleService& titles, Ios::PowerService& power)
    : fileSystem(fileSystem)
    , titles(titles)
    , power(power)
{
}

void SystemMemoryFormatter::Format()
{
    DeleteTitlesOfTheUser();
    DeleteTicketsOfTheUser();
    EmptyDataFolderOfSystemChannels();
    EmptyDataFolderOfTheSystemMenu();
    EmptySharedFolder();
}

void SystemMemoryFormatter::ShutDownConsole()
{
    power.RequestShutdown();
}

void SystemMemoryFormatter::DeleteTitlesOfTheUser()
{
    Ios::Result<std::vector<Ios::TitleId>> installedTitles = titles.ListInstalledTitles();
    if (installedTitles.Failed())
        throw SettingsError(std::format("The installed titles cannot be listed (result code {})", Ios::OriginalResultCode(installedTitles.Error())));

    for (const Ios::TitleId& titleId : installedTitles.Value())
    {
        if (!IsTitleOfTheUser(titleId))
            continue;

        Ios::Result<void> deleted = titles.DeleteTitle(titleId);
        if (deleted.Failed())
            throw SettingsError(std::format("The title {} cannot be deleted (result code {})", titleId.ToString(), Ios::OriginalResultCode(deleted.Error())));
    }
}

void SystemMemoryFormatter::DeleteTicketsOfTheUser()
{
    Ios::Result<std::vector<Ios::TitleId>> titlesWithTickets = titles.ListTitlesWithTickets();
    if (titlesWithTickets.Failed())
        throw SettingsError(std::format("The tickets cannot be listed (result code {})", Ios::OriginalResultCode(titlesWithTickets.Error())));

    for (const Ios::TitleId& titleId : titlesWithTickets.Value())
    {
        if (IsTitleOfTheUser(titleId))
            DeleteEntry(Ios::TicketFilePath(titleId));
    }
}

void SystemMemoryFormatter::EmptyDataFolderOfSystemChannels()
{
    Ios::Result<std::vector<Ios::TitleId>> installedTitles = titles.ListInstalledTitles();
    if (installedTitles.Failed())
        throw SettingsError(std::format("The installed titles cannot be listed (result code {})", Ios::OriginalResultCode(installedTitles.Error())));

    for (const Ios::TitleId& titleId : installedTitles.Value())
    {
        if (IsSystemChannel(titleId))
            EmptyDirectory(Ios::DataFolderPath(titleId), {});
    }
}

void SystemMemoryFormatter::EmptyDataFolderOfTheSystemMenu()
{
    EmptyDirectory(Ios::DataFolderPath(Ios::SystemMenuTitleId), {std::string(ProductInformationFileName)});
}

void SystemMemoryFormatter::EmptySharedFolder()
{
    Ios::NandPath sharedFolder = ParsePath(SharedFolderPath);
    EmptyDirectory(sharedFolder, {std::string(SystemSharedFolderName)});

    Ios::NandPath systemSharedFolder = ChildPath(sharedFolder, std::string(SystemSharedFolderName));
    EmptyDirectory(systemSharedFolder, {std::string(SettingsFileName)});
}

void SystemMemoryFormatter::EmptyDirectory(const Ios::NandPath& directory, const std::vector<std::string>& namesToKeep)
{
    Ios::Result<std::vector<std::string>> names = fileSystem.ListDirectory(directory, Ios::SuperUserCaller);
    if (names.Failed())
    {
        // A data folder that is not there has nothing to erase.
        if (names.Error() == Ios::IosError::NotFound)
            return;

        throw SettingsError(std::format("The folder {} cannot be listed (result code {})", directory.ToString(), Ios::OriginalResultCode(names.Error())));
    }

    for (const std::string& name : names.Value())
    {
        bool isKept = std::find(namesToKeep.begin(), namesToKeep.end(), name) != namesToKeep.end();
        if (!isKept)
            DeleteEntry(ChildPath(directory, name));
    }
}

void SystemMemoryFormatter::DeleteEntry(const Ios::NandPath& path)
{
    Ios::Result<void> deleted = fileSystem.DeleteEntry(path, Ios::SuperUserCaller);
    if (deleted.Failed() && deleted.Error() != Ios::IosError::NotFound)
        throw SettingsError(std::format("{} cannot be deleted (result code {})", path.ToString(), Ios::OriginalResultCode(deleted.Error())));
}

}
