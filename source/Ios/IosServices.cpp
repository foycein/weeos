#include "Ios/IosServices.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NandStorage.h"

#include <stdexcept>
#include <utility>

namespace Ios
{

Result<void> IosServices::Start(const IosFolders& folders)
{
    if (IsRunning())
        return IosError::AlreadyExists;

    Result<NandStorage> storage = NandStorage::Open(folders.nandFolder);
    if (storage.Failed())
        return storage.Error();

    Result<void> prepared = PrepareNandFolder(storage.Value());
    if (prepared.Failed())
        return prepared;

    fileSystem = std::make_unique<FileSystemService>(std::move(storage.Value()));
    return Result<void>();
}

void IosServices::Stop()
{
    if (!IsRunning())
        return;

    fileSystem->Shutdown();
    fileSystem.reset();
}

bool IosServices::IsRunning() const
{
    return fileSystem != nullptr;
}

FileSystemService& IosServices::GetFileSystem()
{
    if (!IsRunning())
        throw std::logic_error("IosServices::GetFileSystem was called while IOS is not running");

    return *fileSystem;
}

}
