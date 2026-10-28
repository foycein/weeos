#pragma once

#include "Ios/DiscDrive/DiscDriveService.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Identity/ConsoleIdentityService.h"
#include "Ios/IosFolders.h"
#include "Ios/Keyboard/KeyboardService.h"
#include "Ios/Power/PowerService.h"
#include "Ios/Remote/RemoteService.h"
#include "Ios/Result.h"
#include "Ios/Title/TitleService.h"

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
    // Brings IOS up. First the identity of the console is read, then the NAND folder is made ready, as the console does every time IOS starts,
    // then the services start. Gives back NoFileSystem when the NAND folder cannot be used and AlreadyExists when the services are running already.
    // The files of the identity folder can be missing (NotFound), unreadable (StorageFailure) or wrong (ShortRead).
    Result<void> Start(const IosFolders& folders);

    // Stops the services. The file system shuts down, so a file that is still open cannot be used any more,
    // and whoever waits for a button or a request stops waiting. The services can be started again afterwards.
    //
    // The service objects stay where they are, so a thread that is still inside one of them can leave safely.
    // They are replaced by the next Start and destroyed with this object.
    void Stop();

    bool IsRunning() const;

    // The services exist while IOS is running. Asking for one at any other time is a mistake in the program and throws.
    FileSystemService& GetFileSystem();
    DiscDriveService& GetDiscDrive();
    KeyboardService& GetKeyboard();
    RemoteService& GetRemote();
    PowerService& GetPower();
    ConsoleIdentityService& GetConsoleIdentity();
    TitleService& GetTitles();

private:
    void RequireRunning() const;

    bool isRunning = false;
    std::unique_ptr<ConsoleIdentityService> identity;
    std::unique_ptr<FileSystemService> fileSystem;
    std::unique_ptr<DiscDriveService> discDrive;
    std::unique_ptr<RemoteService> remote;
    std::unique_ptr<PowerService> power;
    std::unique_ptr<KeyboardService> keyboard;
    std::unique_ptr<TitleService> titles;
};

}
