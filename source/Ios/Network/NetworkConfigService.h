#pragma once

#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Network/MacAddress.h"
#include "Ios/Network/NetworkConfiguration.h"
#include "Ios/Result.h"

#include <cstdint>
#include <mutex>

namespace Ios
{

// The module of IOS that keeps the network configuration runs as this user. It owns the folder /shared2/sys/net and the file in it.
constexpr Caller NetworkConfigCaller = {13, 13};

// What GetLinkStatus tells a program. The numbers are the ones the SDK compares: a program waits while the status is
// Busy and goes on to the sockets when it is 3, 4 or 5.
enum class LinkStatus : std::uint8_t
{
    // The module is busy, or no program asked it to connect yet. Ask again later.
    Busy = 1,

    // No connection of the chosen kind is enabled. The network cannot be used.
    NoInterface = 2,

    // A wired connection is chosen and its interface is ready. The module waits for a cable, which is never plugged in here.
    WiredInterfaceReady = 3,

    // An interface is chosen but the link is not up: the module is connecting, or the link went down.
    LinkDown = 4,

    // The link is up.
    LinkUp = 5,
};

// The kind of connection that the module asked the socket stack to use.
enum class NetworkInterface
{
    None,
    Wired,
    Wireless,
};

// The network configuration module (NCD) of IOS, in place of /dev/net/ncd/manage.
//
// It keeps the three connections of the console in the file /shared2/sys/net/02/config.dat and connects with the one that
// a program chose. The console here has no radio that finds an access point and no network adapter, so no connection is ever
// made. A program that chooses a wireless connection is told to wait for ever (Busy), a program that chooses a wired connection
// gets a wired interface without a link, and with no connection that can be used the module says so (NoInterface).
//
// All calls are thread safe.
class NetworkConfigService
{
public:
    // Reads the stored configuration, the one the module works with, when it starts.
    NetworkConfigService(FileSystemService& fileSystem, const MacAddress& wirelessMacAddress);

    // The configuration file. An older console kept it in /shared2/sys/net/config.dat: that file is moved to the new place first.
    // NotFound when there is no file, StorageFailure when it is not a configuration file.
    Result<NetworkConfiguration> ReadConfiguration();

    // Checks the configuration and stores it in the file, which is made when it is missing, with its folders.
    // InvalidArgument when FindConfigurationProblems finds something.
    Result<void> WriteConfiguration(const NetworkConfiguration& configuration);

    // The configuration the module works with at the moment. At first it is the stored one, or an empty one when none is stored.
    NetworkConfiguration GetCurrentConfiguration() const;

    // A program chooses the connection: the configuration becomes the one the module works with, and the module sets about
    // connecting with it. InvalidArgument when FindConfigurationProblems finds something. The stored file is not changed.
    Result<void> ApplyConfiguration(const NetworkConfiguration& configuration);

    LinkStatus GetLinkStatus() const;

    NetworkInterface GetChosenInterface() const;

    MacAddress GetWirelessMacAddress() const;

    // A program takes the radio for itself, for example to search for access points. Gives back the number that unlocks it again.
    // AlreadyExists while another program has the radio.
    Result<std::uint32_t> LockWirelessDriver();

    // InvalidArgument when the number is not the one of the program that has the radio.
    Result<void> UnlockWirelessDriver(std::uint32_t lockNumber);

private:
    NetworkInterface ChooseInterface() const;

    FileSystemService& fileSystem;
    MacAddress wirelessMacAddress;

    mutable std::mutex mutex;
    NetworkConfiguration currentConfiguration;
    bool hasBeenAskedToConnect = false;
    std::uint32_t lockNumber = 0;
    std::uint32_t lastLockNumber = 0;
};

}
