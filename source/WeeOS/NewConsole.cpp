#include "WeeOS/NewConsole.h"

#include "PersonalSettings.h"
#include "PreparedTitles.h"

#include "Formats/FileBytes.h"
#include "Formats/SettingTextFile.h"
#include "Formats/SystemConfiguration.h"
#include "Ios/FileSystem/HostNames.h"
#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NewEntryDefaults.h"
#include "Ios/Identity/IdentityFiles.h"
#include "Ios/IosServices.h"
#include "Settings/Countries.h"
#include "Settings/SystemSettings.h"

#include <algorithm>
#include <format>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace WeeOS
{

namespace
{

constexpr std::string_view CertificatesFileName = "certificates.bin";
constexpr std::string_view NewNandFolderEnding = " (being made)";

// The System Menu runs as this user and group; the files it keeps belong to it, as on the reference console.
constexpr Ios::Caller SystemMenuOwner = {Ios::SystemMenuUserId, Ios::SystemMenuGroupId};

// The network configuration module owns its folders and its file (user and group 13 on the reference console).
constexpr Ios::Caller NetworkModuleOwner = {13, 13};

constexpr Ios::EntryPermissions ReadOnlyForEveryone = {Ios::AccessRights::Read, Ios::AccessRights::Read, Ios::AccessRights::Read};
constexpr Ios::EntryPermissions ReadWriteForSystem = {Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite, Ios::AccessRights::Read};

// The facts about the console in setting.txt (docs/research/ConsoleDump.md 6): a console of the USA, the model of the reference console.
// Assumed: the serial number is made for the install, nine digits after the code of its factory.
constexpr std::string_view FactoryCode = "LU";
constexpr int SerialNumberDigitCount = 9;

// The settings of a new WeeOS, in the order of the reference console's SYSCONF. Arrays start with zeros. The values are the ones the
// first start of the System Menu writes (docs/research/ConsoleDump.md 6.1) and the owner's choices (docs/DECISIONS.md): English,
// 16:9, the sensor bar below the television, the setup done (WeeOS has no setup pages), the user agreements accepted.
// Assumed: IPL.UPT, NET.WCFG and IPL.IDL as the reference console has them; nothing in WeeOS reads them.
struct NewSetting
{
    std::string_view name;
    Formats::SettingType type;
    std::uint64_t number;
    std::size_t arraySize;
};

constexpr std::uint64_t PointerSensitivity = 3;
constexpr std::uint64_t WidescreenAspectRatio = 1;
constexpr std::uint64_t SensorBarBelow = 0;
constexpr std::uint64_t EnglishLanguage = 1;
constexpr std::uint64_t SpeakerVolume = 0x58;
constexpr std::uint64_t Yes = 1;
constexpr std::uint64_t No = 0;

constexpr NewSetting NewSettings[] = {
    {"BT.DINF", Formats::SettingType::BigArray, 0, 1121},
    {"BT.SENS", Formats::SettingType::Long, PointerSensitivity, 0},
    {"IPL.NIK", Formats::SettingType::SmallArray, 0, 22},
    {"IPL.AR", Formats::SettingType::Byte, WidescreenAspectRatio, 0},
    {"BT.BAR", Formats::SettingType::Byte, SensorBarBelow, 0},
    {"IPL.SSV", Formats::SettingType::Byte, No, 0},
    {"IPL.LNG", Formats::SettingType::Byte, EnglishLanguage, 0},
    {"IPL.SADR", Formats::SettingType::BigArray, 0, 4104},
    {"IPL.CB", Formats::SettingType::Long, 0, 0},
    {"BT.SPKV", Formats::SettingType::Byte, SpeakerVolume, 0},
    {"IPL.PC", Formats::SettingType::SmallArray, 0, 74},
    {"NET.CTPC", Formats::SettingType::Long, 0, 0},
    {"WWW.RST", Formats::SettingType::Bool, No, 0},
    {"BT.CDIF", Formats::SettingType::BigArray, 0, 517},
    {"IPL.INC", Formats::SettingType::Long, 0, 0},
    {"IPL.FRC", Formats::SettingType::Long, 0, 0},
    {"IPL.CD", Formats::SettingType::Bool, Yes, 0},
    {"IPL.CD2", Formats::SettingType::Bool, Yes, 0},
    {"IPL.UPT", Formats::SettingType::Byte, 1, 0},
    {"BT.MOT", Formats::SettingType::Byte, Yes, 0},
    {"IPL.EULA", Formats::SettingType::Bool, Yes, 0},
    {"NET.WCFG", Formats::SettingType::Long, 1, 0},
    {"IPL.IDL", Formats::SettingType::SmallArray, 0, 2},
};

constexpr std::string_view AddressSettingName = "IPL.SADR";
constexpr std::string_view IdleSettingName = "IPL.IDL";
constexpr Formats::Byte IdleSettingByte = 1;

void Require(const Ios::Result<void>& result, std::string_view step)
{
    if (result.Failed())
        throw std::runtime_error(std::format("A new console cannot be made: {} failed (result code {})", step,
            Ios::OriginalResultCode(result.Error())));
}

Ios::NandPath MakePath(std::string_view text)
{
    Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(text);
    if (path.Failed())
        throw std::logic_error(std::format("The path {} is not a NAND path", text));

    return path.Value();
}

void MakeDirectory(Ios::FileSystemService& fileSystem, std::string_view path, const Ios::Caller& owner)
{
    Ios::NandPath nandPath = MakePath(path);
    Ios::Result<void> created = fileSystem.CreateDirectory(nandPath, Ios::ReadWriteForEveryone, Ios::NoEntryFlags, Ios::SuperUserCaller);
    Require(created, std::format("making {}", path));
    Ios::EntryAttributes attributes{owner.userId, owner.groupId, Ios::ReadWriteForEveryone, Ios::NoEntryFlags};
    Require(fileSystem.SetAttributes(nandPath, attributes, Ios::SuperUserCaller), std::format("giving {} to its owner", path));
}

// The kernel makes the file and gives it to its owner while it is empty, then writes it: the super user may do both.
void WriteFile(Ios::FileSystemService& fileSystem, std::string_view path, Formats::ByteView bytes, const Ios::Caller& owner,
    const Ios::EntryPermissions& permissions)
{
    Ios::NandPath nandPath = MakePath(path);
    Require(fileSystem.CreateFile(nandPath, permissions, Ios::NoEntryFlags, Ios::SuperUserCaller), std::format("making {}", path));
    Ios::EntryAttributes attributes{owner.userId, owner.groupId, permissions, Ios::NoEntryFlags};
    Require(fileSystem.SetAttributes(nandPath, attributes, Ios::SuperUserCaller), std::format("giving {} to its owner", path));

    Ios::Result<Ios::OpenFile> file = fileSystem.Open(nandPath, Ios::OpenMode::Write, Ios::SuperUserCaller);
    if (file.Failed())
        Require(Ios::Result<void>(file.Error()), std::format("opening {}", path));

    Ios::Result<std::size_t> written = file.Value().Write(bytes);
    if (written.Failed())
        Require(Ios::Result<void>(written.Error()), std::format("writing {}", path));
}

// The System Menu first, so that it gets the first user id as on every Wii, then the folders in the order of their names.
std::vector<std::filesystem::path> ListPreparedTitles(const std::filesystem::path& wiiFilesFolder)
{
    std::vector<std::filesystem::path> titleFolders;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(wiiFilesFolder))
    {
        if (entry.is_directory())
            titleFolders.push_back(entry.path());
    }

    std::sort(titleFolders.begin(), titleFolders.end());
    if (titleFolders.empty())
        throw std::runtime_error(std::format("A new console cannot be made: the Wii files are not in {}", wiiFilesFolder.string()));

    return titleFolders;
}

void InstallTitles(Ios::TitleService& titles, const std::filesystem::path& wiiFilesFolder)
{
    for (const std::filesystem::path& titleFolder : ListPreparedTitles(wiiFilesFolder))
        Require(InstallPreparedTitle(titles, titleFolder), std::format("installing the title {}", titleFolder.filename().string()));
}

std::string MakeSerialNumber()
{
    std::random_device randomDevice;
    std::uniform_int_distribution<int> anyDigit(0, 9);

    std::string serialNumber;
    for (int digit = 0; digit < SerialNumberDigitCount; ++digit)
        serialNumber += static_cast<char>('0' + anyDigit(randomDevice));

    return serialNumber;
}

Formats::Bytes MakeSettingText()
{
    Formats::SettingTextFile file;
    file.entries = {
        {"AREA", "USA"},
        {"MODEL", "RVL-101(USA)"},
        {"DVD", "0"},
        {"MPCH", "0x7FFE"},
        {"CODE", std::string(FactoryCode)},
        {"SERNO", MakeSerialNumber()},
        {"VIDEO", "NTSC"},
        {"GAME", "US"},
    };
    return Formats::WriteSettingTextFile(file);
}

Formats::Setting* FindSetting(Formats::SystemConfiguration& configuration, std::string_view name)
{
    for (Formats::Setting& setting : configuration.settings)
    {
        if (setting.name == name)
            return &setting;
    }
    return nullptr;
}

// The settings of a console that nobody used (ResetPersonalSettings), in the country of the console.
Formats::Bytes MakeSystemConfiguration()
{
    Formats::SystemConfiguration configuration;
    for (const NewSetting& setting : NewSettings)
        configuration.settings.push_back({std::string(setting.name), setting.type, setting.number, Formats::Bytes(setting.arraySize, 0)});

    ResetPersonalSettings(configuration);

    Formats::Setting* address = FindSetting(configuration, AddressSettingName);
    address->bytes.front() = Settings::UnitedStatesCode;

    Formats::Setting* idle = FindSetting(configuration, IdleSettingName);
    std::fill(idle->bytes.begin(), idle->bytes.end(), IdleSettingByte);

    return Formats::WriteSystemConfiguration(configuration);
}

void WriteSystemFiles(Ios::FileSystemService& fileSystem, const std::filesystem::path& wiiFilesFolder)
{
    std::filesystem::path menuFolder = wiiFilesFolder / std::format("{:016x}", Ios::SystemMenuTitleId.ToValue());
    Formats::Bytes certificates = Formats::ReadFileBytes(menuFolder / CertificatesFileName);
    WriteFile(fileSystem, "/sys/cert.sys", certificates, Ios::SuperUserCaller, ReadWriteForSystem);

    WriteFile(fileSystem, "/title/00000001/00000002/data/setting.txt", MakeSettingText(), SystemMenuOwner, ReadOnlyForEveryone);

    MakeDirectory(fileSystem, "/shared2/sys", SystemMenuOwner);
    WriteFile(fileSystem, "/shared2/sys/SYSCONF", MakeSystemConfiguration(), SystemMenuOwner, Ios::ReadWriteForEveryone);

    MakeDirectory(fileSystem, "/shared2/sys/net", NetworkModuleOwner);
    MakeDirectory(fileSystem, "/shared2/sys/net/02", NetworkModuleOwner);
    Formats::Bytes networkConfiguration = MakeWeeOSNetworkConfigurationFile();
    WriteFile(fileSystem, "/shared2/sys/net/02/config.dat", networkConfiguration, NetworkModuleOwner, Ios::ReadWriteForEveryone);
}

void GiveTheConsoleItsName(Ios::FileSystemService& fileSystem)
{
    Settings::SystemSettings settings(fileSystem, SystemMenuOwner);
    settings.SetNickname(std::u16string(Settings::FreshConsoleNickname));
    settings.Save();
}

}

bool HasConsole(const Ios::IosFolders& folders)
{
    return std::filesystem::exists(folders.nandFolder / Ios::IndexFileName);
}

// The console is made in a folder of its own beside the NAND folder and gets the name of the NAND folder when it is complete,
// so that a start that stops half way leaves no console that looks finished.
void MakeNewConsole(const Ios::IosFolders& folders, const std::filesystem::path& wiiFilesFolder)
{
    if (!std::filesystem::exists(folders.identityFolder / Ios::KeysFileName))
        Ios::WriteNewIdentity(Ios::FindSharedKeysInWiiFiles(wiiFilesFolder), folders.identityFolder);

    Ios::IosFolders newFolders = folders;
    newFolders.nandFolder += NewNandFolderEnding;
    std::filesystem::remove_all(newFolders.nandFolder);
    std::filesystem::create_directories(newFolders.nandFolder);

    Ios::IosServices services;
    Require(services.Start(newFolders), "starting IOS on the empty NAND");
    InstallTitles(services.GetTitles(), wiiFilesFolder);
    WriteSystemFiles(services.GetFileSystem(), wiiFilesFolder);
    GiveTheConsoleItsName(services.GetFileSystem());
    services.Stop();

    std::filesystem::remove(folders.nandFolder);
    std::filesystem::rename(newFolders.nandFolder, folders.nandFolder);
}

}
