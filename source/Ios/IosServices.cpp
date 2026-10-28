#include "Ios/IosServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NandStorage.h"

#include <stdexcept>
#include <utility>

namespace Ios
{

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

    // The order of the modules of IOS80: file system, DIP (disc drive), OH1 (the Bluetooth device of the remotes), STM (power), KBD (keyboard).
    fileSystem = std::make_unique<FileSystemService>(std::move(storage.Value()));
    discDrive = std::make_unique<DiscDriveService>();
    remote = std::make_unique<RemoteService>();
    power = std::make_unique<PowerService>();
    keyboard = std::make_unique<KeyboardService>();
    isRunning = true;
    return Result<void>();
}

void IosServices::Stop()
{
    if (!isRunning)
        return;

    keyboard->Close();
    power->Close();
    remote->Close();
    fileSystem->Shutdown();
    isRunning = false;
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
