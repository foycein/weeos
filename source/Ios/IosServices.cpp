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

IosServices::IosServices(PcConnectionCheck pcConnectionCheck)
    : isPcConnected(std::move(pcConnectionCheck))
{
}

Result<void> IosServices::Start(const IosFolders& folders, const IosProfile& startProfile)
{
    if (isRunning)
        return IosError::AlreadyExists;

    Result<ConsoleIdentityService> consoleIdentity = ConsoleIdentityService::Open(folders.identityFolder);
    if (consoleIdentity.Failed())
        return consoleIdentity.Error();

    Result<NandStorage> storage = NandStorage::Open(folders.nandFolder);
    if (storage.Failed())
        return storage.Error();

    Result<void> prepared = PrepareNandFolder(storage.Value(), startProfile);
    if (prepared.Failed())
        return prepared;

    Result<MacAddress> wirelessMacAddress = ReadWirelessMacAddress(folders.identityFolder);
    if (wirelessMacAddress.Failed())
        return wirelessMacAddress.Error();

    DestroyServices();
    identity = std::make_unique<ConsoleIdentityService>(std::move(consoleIdentity.Value()));

    // The order of the modules of IOS80: file system, DIP (disc drive), OH1 (the Bluetooth device of the remotes), SDI (SD card),
    // WD (wireless), NCD (network configuration), SO (sockets), STM (power), KBD (keyboard), SSL (secure sockets). The Wii Speak microphone
    // is a device on the USB port of OH0, like the keyboard, and starts after it.
    // The sockets use the network configuration, so they start after it.
    fileSystem = std::make_unique<FileSystemService>(std::move(storage.Value()), startProfile);
    titles = std::make_unique<TitleService>(*fileSystem, *identity, startProfile);
    titles->SettleInterruptedInstallations();
    discDrive = std::make_unique<DiscDriveService>();
    remote = std::make_unique<RemoteService>();
    sdCard = std::make_unique<SdCardService>(startProfile);
    PutCardIntoSlot(*sdCard, folders);
    wireless = std::make_unique<WirelessService>(wirelessMacAddress.Value());
    networkConfig = std::make_unique<NetworkConfigService>(*fileSystem, wirelessMacAddress.Value(), isPcConnected, startProfile);
    sockets = std::make_unique<SocketService>(*networkConfig);
    power = std::make_unique<PowerService>();
    keyboard = std::make_unique<KeyboardService>(startProfile);
    microphone = std::make_unique<MicrophoneService>();
    secureSockets = std::make_unique<SecureSocketService>(*sockets);
    profile = startProfile;
    isRunning = true;
    return Result<void>();
}

void IosServices::Stop()
{
    if (!isRunning)
        return;

    microphone->Close();
    keyboard->Close();
    power->Close();
    sockets->Cleanup();
    remote->Close();
    sdCard->Close();
    fileSystem->Shutdown();
    isRunning = false;
}

// The services of an earlier start go first, the ones that use another service before that service.
void IosServices::DestroyServices()
{
    secureSockets.reset();
    microphone.reset();
    keyboard.reset();
    power.reset();
    sockets.reset();
    networkConfig.reset();
    wireless.reset();
    sdCard.reset();
    remote.reset();
    discDrive.reset();
    titles.reset();
    fileSystem.reset();
    identity.reset();
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

KeyboardService& IosServices::GetKeyboard()
{
    RequireRunning();
    return *keyboard;
}

RemoteService& IosServices::GetRemote()
{
    RequireRunning();
    return *remote;
}

MicrophoneService& IosServices::GetMicrophone()
{
    RequireRunning();
    return *microphone;
}

PowerService& IosServices::GetPower()
{
    RequireRunning();
    return *power;
}

ConsoleIdentityService& IosServices::GetConsoleIdentity()
{
    RequireRunning();
    return *identity;
}

TitleService& IosServices::GetTitles()
{
    RequireRunning();
    return *titles;
}

const IosProfile& IosServices::GetProfile() const
{
    RequireRunning();
    return profile;
}

void IosServices::RequireRunning() const
{
    if (!isRunning)
        throw std::logic_error("A service of IOS was asked for while IOS is not running");
}

}
