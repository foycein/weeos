#include "Ios/Network/NetworkConfigService.h"

#include "Ios/FileSystem/NewEntryDefaults.h"

#include <algorithm>
#include <span>
#include <string_view>

namespace Ios
{

namespace
{

// The folders in front of the file, from the outside to the inside. The module makes the ones that are missing.
constexpr std::string_view FolderPaths[] = {"/shared2/sys", "/shared2/sys/net", "/shared2/sys/net/02"};
constexpr std::string_view NewFilePath = "/shared2/sys/net/02/config.dat";

// The place of the file in the module of IOS9 and in the older consoles.
constexpr std::string_view OldFolderPaths[] = {"/shared2/sys", "/shared2/sys/net"};
constexpr std::string_view OldFilePath = "/shared2/sys/net/config.dat";

NandPath PathOf(std::string_view text)
{
    return NandPath::Parse(text).Value();
}

bool Exists(const FileSystemService& fileSystem, const NandPath& path)
{
    return fileSystem.GetAttributes(path, NetworkConfigCaller).Succeeded();
}

Result<void> MakeMissingFolders(FileSystemService& fileSystem, std::span<const std::string_view> folderPaths)
{
    for (std::string_view folderPath : folderPaths)
    {
        NandPath folder = PathOf(folderPath);
        if (Exists(fileSystem, folder))
            continue;

        Result<void> made = fileSystem.CreateDirectory(folder, ReadWriteForEveryone, NoEntryFlags, NetworkConfigCaller);
        if (made.Failed())
            return made;
    }
    return Result<void>();
}

// An older console kept the file in the folder of the network. The module moves it into the new folder and uses the new file.
// If the move does not work it keeps using the old file.
NandPath MoveOldFileIntoPlace(FileSystemService& fileSystem)
{
    NandPath newFile = PathOf(NewFilePath);
    NandPath oldFile = PathOf(OldFilePath);
    if (!Exists(fileSystem, oldFile))
        return newFile;

    if (Exists(fileSystem, newFile))
        fileSystem.DeleteEntry(newFile, NetworkConfigCaller);

    if (MakeMissingFolders(fileSystem, FolderPaths).Failed())
        return oldFile;

    if (fileSystem.RenameEntry(oldFile, newFile, NetworkConfigCaller).Failed())
        return oldFile;

    return newFile;
}

Result<Formats::Bytes> ReadWholeFile(FileSystemService& fileSystem, const NandPath& path)
{
    Result<OpenFile> opened = fileSystem.Open(path, OpenMode::Read, NetworkConfigCaller);
    if (opened.Failed())
        return opened.Error();

    // One byte more than the configuration has: a longer file is not a configuration file.
    return opened.Value().Read(NetworkConfigurationFileSize + 1);
}

std::span<const std::string_view> FolderPathsOf(NetworkConfigurationKind kind)
{
    if (kind == NetworkConfigurationKind::Original)
        return OldFolderPaths;

    return FolderPaths;
}

}

NetworkConfigService::NetworkConfigService(FileSystemService& fileSystemToUse, const MacAddress& macAddress, const IosProfile& iosProfile)
    : fileSystem(fileSystemToUse)
    , wirelessMacAddress(macAddress)
    , profile(iosProfile)
{
    Result<NetworkConfiguration> stored = ReadConfiguration();
    if (stored.Succeeded())
        currentConfiguration = stored.Value();
}

NandPath NetworkConfigService::FindConfigurationFile()
{
    if (profile.networkConfiguration == NetworkConfigurationKind::Original)
        return PathOf(OldFilePath);

    return MoveOldFileIntoPlace(fileSystem);
}

std::vector<std::string> NetworkConfigService::FindProblems(const NetworkConfiguration& configuration) const
{
    bool checksProxies = profile.networkConfiguration == NetworkConfigurationKind::WithProxyChecks;
    return FindConfigurationProblems(configuration, checksProxies);
}

Result<NetworkConfiguration> NetworkConfigService::ReadConfiguration()
{
    NandPath file = FindConfigurationFile();

    Result<Formats::Bytes> bytes = ReadWholeFile(fileSystem, file);
    if (bytes.Failed())
        return bytes.Error();

    return DecodeNetworkConfiguration(bytes.Value());
}

Result<void> NetworkConfigService::WriteConfiguration(const NetworkConfiguration& configuration)
{
    if (!FindProblems(configuration).empty())
        return IosError::InvalidArgument;

    NandPath file = FindConfigurationFile();
    if (!Exists(fileSystem, file))
    {
        Result<void> madeFolders = MakeMissingFolders(fileSystem, FolderPathsOf(profile.networkConfiguration));
        if (madeFolders.Failed())
            return madeFolders;

        Result<void> created = fileSystem.CreateFile(file, ReadWriteForEveryone, NoEntryFlags, NetworkConfigCaller);
        if (created.Failed())
            return created;
    }

    Result<OpenFile> opened = fileSystem.Open(file, OpenMode::Write, NetworkConfigCaller);
    if (opened.Failed())
        return opened.Error();

    Result<std::size_t> written = opened.Value().Write(EncodeNetworkConfiguration(configuration));
    if (written.Failed())
        return written.Error();

    return Result<void>();
}

NetworkConfiguration NetworkConfigService::GetCurrentConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return currentConfiguration;
}

Result<void> NetworkConfigService::ApplyConfiguration(const NetworkConfiguration& configuration)
{
    if (!FindProblems(configuration).empty())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(mutex);
    currentConfiguration = configuration;
    hasBeenAskedToConnect = true;
    return Result<void>();
}

// The caller holds the lock. The module takes the kind of connection that the program asked for, but only when
// one of the three connections of that kind is enabled.
NetworkInterface NetworkConfigService::ChooseInterface() const
{
    if (!hasBeenAskedToConnect)
        return NetworkInterface::None;

    const auto& profiles = currentConfiguration.profiles;
    bool hasWiredConnection = std::any_of(profiles.begin(), profiles.end(), IsUsableWiredConnection);
    bool hasWirelessConnection = std::any_of(profiles.begin(), profiles.end(), IsUsableWirelessConnection);

    if (currentConfiguration.selectedMedia == SelectedMedia::Wired && hasWiredConnection)
        return NetworkInterface::Wired;

    if (currentConfiguration.selectedMedia == SelectedMedia::Wireless && hasWirelessConnection)
        return NetworkInterface::Wireless;

    return NetworkInterface::None;
}

LinkStatus NetworkConfigService::GetLinkStatus() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasBeenAskedToConnect)
        return LinkStatus::Busy;

    NetworkInterface interfaceToUse = ChooseInterface();
    if (interfaceToUse == NetworkInterface::Wired)
        return LinkStatus::WiredInterfaceReady;

    if (interfaceToUse == NetworkInterface::Wireless)
        return LinkStatus::Busy;

    return LinkStatus::NoInterface;
}

NetworkInterface NetworkConfigService::GetChosenInterface() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return ChooseInterface();
}

MacAddress NetworkConfigService::GetWirelessMacAddress() const
{
    return wirelessMacAddress;
}

Result<std::uint32_t> NetworkConfigService::LockWirelessDriver()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (lockNumber != 0)
        return IosError::AlreadyExists;

    ++lastLockNumber;
    lockNumber = lastLockNumber;
    return lockNumber;
}

Result<void> NetworkConfigService::UnlockWirelessDriver(std::uint32_t numberOfTheProgram)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (lockNumber == 0 || numberOfTheProgram != lockNumber)
        return IosError::InvalidArgument;

    lockNumber = 0;
    return Result<void>();
}

}
