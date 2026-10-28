#pragma once

#include "Ios/Clock.h"
#include "Ios/DiscDrive/DiscDriveService.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/IosFolders.h"
#include "Ios/Network/NetworkConfigService.h"
#include "Ios/Network/WirelessService.h"
#include "Ios/Power/PowerService.h"
#include "Ios/Result.h"
#include "Ios/SdCard/SdCardService.h"
#include "Ios/Sockets/SecureSocketService.h"
#include "Ios/Sockets/SocketService.h"
#include "Ios/WiiConnect24/WiiConnect24Service.h"

#include <memory>

namespace Ios
{

// The services of the Wii's operating system IOS, rebuilt as plain classes that the System Menu calls directly.
// This class starts them in the order the console starts its modules and stops them in the opposite order.
// A service that uses another service starts after it.
//
// Not thread safe: the program starts and stops the services from one thread.
class IosServices
{
public:
    // Brings IOS up. First the NAND folder is made ready, as the console does every time IOS starts, then the services start.
    // Gives back NoFileSystem when the NAND folder cannot be used and AlreadyExists when the services are running already.
    // NotFound or InvalidArgument when the identity folder is named but its keys.txt is missing or cannot be read.
    // The folders and files of WiiConnect24 are made on the way, so the NAND must have room for them (about 9 MiB).
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
    SdCardService& GetSdCard();
    WirelessService& GetWireless();
    NetworkConfigService& GetNetworkConfig();
    SocketService& GetSockets();
    SecureSocketService& GetSecureSockets();
    WiiConnect24Service& GetWiiConnect24();
    PowerService& GetPower();

private:
    void RequireRunning() const;
    void DestroyServices();

    bool isRunning = false;
    SystemClock clock;
    std::unique_ptr<FileSystemService> fileSystem;
    std::unique_ptr<DiscDriveService> discDrive;
    std::unique_ptr<SdCardService> sdCard;
    std::unique_ptr<WirelessService> wireless;
    std::unique_ptr<NetworkConfigService> networkConfig;
    std::unique_ptr<SocketService> sockets;
    std::unique_ptr<WiiConnect24Service> wiiConnect24;
    std::unique_ptr<PowerService> power;
    std::unique_ptr<SecureSocketService> secureSockets;
};

}
