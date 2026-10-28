#include "Ios/IosServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NandStorage.h"

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

    isRunning = true;
    return Result<void>();
}

void IosServices::Stop()
{
    isRunning = false;
}

bool IosServices::IsRunning() const
{
    return isRunning;
}

}
