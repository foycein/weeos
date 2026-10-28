#include "Ios/IosServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NandStorage.h"

#include <stdexcept>
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

    fileSystem = std::make_unique<FileSystemService>(std::move(storage.Value()));
    discDrive = std::make_unique<DiscDriveService>();
    sdCard = std::make_unique<SdCardService>();
    PutCardIntoSlot(*sdCard, folders);
    power = std::make_unique<PowerService>();
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
