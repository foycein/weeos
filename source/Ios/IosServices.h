#pragma once

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/IosFolders.h"
#include "Ios/Result.h"

#include <memory>

namespace Ios
{

// The services of the Wii's operating system IOS, rebuilt as plain classes that the System Menu calls directly.
// This class starts them in the order the console starts its modules and stops them in the opposite order.
//
// Not thread safe: the program starts and stops the services from one thread.
class IosServices
{
public:
    // Brings IOS up. First the NAND folder is made ready, as the console does every time IOS starts, then the services start.
    // Gives back NoFileSystem when the NAND folder cannot be used and AlreadyExists when the services are running already.
    Result<void> Start(const IosFolders& folders);

    // Stops the services. The file system shuts down first, so a file that is still open cannot be used any more.
    // The services can be started again afterwards.
    void Stop();

    bool IsRunning() const;

    // The services exist while IOS is running. Asking for one at any other time is a mistake in the program and throws.
    // A reference is not valid any more after Stop.
    FileSystemService& GetFileSystem();

private:
    std::unique_ptr<FileSystemService> fileSystem;
};

}
