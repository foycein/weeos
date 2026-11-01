#pragma once

#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Network/MacAddress.h"
#include "Ios/Network/NetworkConfiguration.h"
#include "Ios/Network/PcConnection.h"
#include "Ios/Profile/ProfileChoice.h"
#include "Ios/Result.h"

#include <cstdint>
#include <mutex>

namespace Ios
{

// The module of IOS that keeps the network configuration runs as this user. It owns the folder /shared2/sys/net and the file in it.
constexpr std::uint32_t NetworkConfigUserId = 13;
constexpr std::uint16_t NetworkConfigGroupId = 13;
constexpr Caller NetworkConfigCaller = {NetworkConfigUserId, NetworkConfigGroupId};

// What GetLinkStatus tells a program. The numbers are the ones the SDK compares: a program waits while the status is
// Busy and goes on to the sockets when it is 3, 4 or 5.
enum class LinkStatus : std::uint8_t
{
    // The module is busy, or no program asked it to connect yet. Ask again later.
    Busy = 1,

    // No connection of the chosen kind is enabled. The network cannot be used.
    NoInterface = 2,

    // A wired connection is chosen and its interface is ready, but no cable is plugged in.
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
// The console is online through the PC's connection, with nothing to choose (docs/DECISIONS.md, Online). The PC's connection is the
// console's one wired connection: when a program asks the module to connect, the link is up while the PC is connected, and while
// it is not the wired interface waits for a cable that is not plugged in. The three connections of the file
// /shared2/sys/net/02/config.dat are still read and written as the original does, but they decide nothing.
//
// The module of IOS9 is older: it keeps the file in /shared2/sys/net/config.dat and does not check the proxy settings.
//
// All calls are thread safe.
class NetworkConfigService
{
public:
    // Reads the stored configuration, the one the module works with, when it starts. The check tells whether the PC is connected.
    // Without a profile this is the module of IOS80.
    NetworkConfigService(FileSystemService& fileSystem, const MacAddress& wirelessMacAddress, PcConnectionCheck isPcConnected,
        const IosProfile& iosProfile = SystemMenuProfile());

    // The configuration file. An older console kept it in /shared2/sys/net/config.dat: that file is moved to the new place first
    // (not by IOS9, which keeps it there). NotFound when there is no file, StorageFailure when it is not a configuration file.
    Result<NetworkConfiguration> ReadConfiguration();

    // Checks the configuration and stores it in the file, which is made when it is missing, with its folders.
    // InvalidArgument when FindConfigurationProblems finds something.
    Result<void> WriteConfiguration(const NetworkConfiguration& configuration);

    // The configuration the module works with at the moment. At first it is the stored one, or an empty one when none is stored.
    NetworkConfiguration GetCurrentConfiguration() const;

    // A program asks the module to connect with the configuration, which becomes the one the module works with. The module connects
    // through the PC whatever the configuration says. InvalidArgument when FindConfigurationProblems finds something.
    // The stored file is not changed.
    Result<void> ApplyConfiguration(const NetworkConfiguration& configuration);

    // Busy until a program asked the module to connect; then LinkUp while the PC is connected and WiredInterfaceReady while it is not.
    LinkStatus GetLinkStatus() const;

    // Nothing until a program asked the module to connect, then the wired interface: the PC's connection.
    NetworkInterface GetChosenInterface() const;

    MacAddress GetWirelessMacAddress() const;

    // A program takes the radio for itself, for example to search for access points. Gives back the number that unlocks it again.
    // AlreadyExists while another program has the radio.
    Result<std::uint32_t> LockWirelessDriver();

    // InvalidArgument when the number is not the one of the program that has the radio.
    Result<void> UnlockWirelessDriver(std::uint32_t lockNumber);

private:
    NandPath FindConfigurationFile();
    std::vector<std::string> FindProblems(const NetworkConfiguration& configuration) const;

    FileSystemService& fileSystem;
    MacAddress wirelessMacAddress;
    PcConnectionCheck isPcConnected;
    const IosProfile profile;

    mutable std::mutex mutex;
    NetworkConfiguration currentConfiguration;
    bool hasBeenAskedToConnect = false;
    std::uint32_t lockNumber = 0;
    std::uint32_t lastLockNumber = 0;
};

}
