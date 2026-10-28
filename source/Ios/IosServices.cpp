#include "Ios/IosServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NandStorage.h"
#include "Ios/Network/WirelessMacAddress.h"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace Ios
{

namespace
{

// The card is in the slot when the folder of the card is there. Without the folder the slot stays empty.
void PutCardIntoSlot(SdCardService& sdCard, const IosFolders& folders)
{
    std::error_code error;
    if (!std::filesystem::is_directory(folders.sdCardFolder, error))
        return;

    constexpr bool isWriteProtected = false;
    sdCard.InsertCard(folders.sdCardFolder, isWriteProtected);
}

}

Result<void> IosServices::Start(const IosFolders& folders)
{
    if (isRunning)
        return IosError::AlreadyExists;

    Result<NandStorage> storage = NandStorage::Open(folders.nandFolder);
    if (storage.Failed())
        return storage.Error();

    Result<void> prepared = PrepareNandFolder(storage.Value());
    if (prepared.Failed())
        return prepared;

    Result<MacAddress> wirelessMacAddress = ReadWirelessMacAddress(folders.identityFolder);
    if (wirelessMacAddress.Failed())
        return wirelessMacAddress.Error();

    DestroyServices();
    fileSystem = std::make_unique<FileSystemService>(std::move(storage.Value()));
    discDrive = std::make_unique<DiscDriveService>();
    sdCard = std::make_unique<SdCardService>();
    PutCardIntoSlot(*sdCard, folders);
    wireless = std::make_unique<WirelessService>(wirelessMacAddress.Value());
    networkConfig = std::make_unique<NetworkConfigService>(*fileSystem, wirelessMacAddress.Value());
    sockets = std::make_unique<SocketService>(*networkConfig);
    wiiConnect24 = std::make_unique<WiiConnect24Service>(*fileSystem, clock);
    Result<void> filesPrepared = wiiConnect24->PrepareFiles();
    if (filesPrepared.Failed())
        return filesPrepared;

    power = std::make_unique<PowerService>();
    secureSockets = std::make_unique<SecureSocketService>();
    isRunning = true;
    return Result<void>();
}

void IosServices::Stop()
{
    if (!isRunning)
        return;

    power->Close();
    sdCard->Close();
    fileSystem->Shutdown();
    isRunning = false;
}

// The services of an earlier start go first, the ones that use another service before that service.
void IosServices::DestroyServices()
{
    secureSockets.reset();
    power.reset();
    wiiConnect24.reset();
    sockets.reset();
    networkConfig.reset();
    wireless.reset();
    sdCard.reset();
    discDrive.reset();
    fileSystem.reset();
}

bool IosServices::IsRunning() const
{
    return isRunning;
}

FileSystemService& IosServices::GetFileSystem()
{
    RequireRunning();
    return *fileSystem;
}

DiscDriveService& IosServices::GetDiscDrive()
{
    RequireRunning();
    return *discDrive;
}

SdCardService& IosServices::GetSdCard()
{
    RequireRunning();
    return *sdCard;
}

WirelessService& IosServices::GetWireless()
{
    RequireRunning();
    return *wireless;
}

NetworkConfigService& IosServices::GetNetworkConfig()
{
    RequireRunning();
    return *networkConfig;
}

SocketService& IosServices::GetSockets()
{
    RequireRunning();
    return *sockets;
}

SecureSocketService& IosServices::GetSecureSockets()
{
    RequireRunning();
    return *secureSockets;
}

WiiConnect24Service& IosServices::GetWiiConnect24()
{
    RequireRunning();
    return *wiiConnect24;
}

PowerService& IosServices::GetPower()
{
    RequireRunning();
    return *power;
}

void IosServices::RequireRunning() const
{
    if (!isRunning)
        throw std::logic_error("A service of IOS was asked for while IOS is not running");
}

}
