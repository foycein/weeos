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

    Result<ConsoleIdentityService> consoleIdentity = ConsoleIdentityService::Open(folders.identityFolder);
    if (consoleIdentity.Failed())
        return consoleIdentity.Error();

    Result<NandStorage> storage = NandStorage::Open(folders.nandFolder);
    if (storage.Failed())
        return storage.Error();

    Result<void> prepared = PrepareNandFolder(storage.Value());
    if (prepared.Failed())
        return prepared;

    identity = std::make_unique<ConsoleIdentityService>(std::move(consoleIdentity.Value()));
    fileSystem = std::make_unique<FileSystemService>(std::move(storage.Value()));
    titles = std::make_unique<TitleService>(*fileSystem, *identity);
    discDrive = std::make_unique<DiscDriveService>();
    power = std::make_unique<PowerService>();
    isRunning = true;
    return Result<void>();
}

void IosServices::Stop()
{
    if (!isRunning)
        return;

    power->Close();
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

void IosServices::RequireRunning() const
{
    if (!isRunning)
        throw std::logic_error("A service of IOS was asked for while IOS is not running");
}

}
