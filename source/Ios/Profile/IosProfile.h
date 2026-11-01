#pragma once

#include "Ios/Profile/IosFeature.h"
#include "Ios/Profile/IosFeatureTable.h"
#include "Ios/Profile/IosNumber.h"
#include "Ios/Profile/IosVersionList.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Ios
{

// A range of addresses: from the start up to, but not including, the end.
struct AddressRange
{
    std::uint32_t start;
    std::uint32_t end;

    bool operator==(const AddressRange&) const = default;
};

// What the kernel tells the PowerPC about the second memory (MEM2), in the low memory block that Boot writes.
// The names are those of docs/research/BootChain.md, appendix B.
struct KernelMemoryAddresses
{
    // The end of the MEM2 that the PowerPC can address (0x80003120).
    std::uint32_t mem2End;

    // The part of MEM2 that the PowerPC may use (0x80003124 and 0x80003128).
    AddressRange mem2Usable;

    // Where IOS and the PowerPC pass requests to each other (0x80003130 and 0x80003134).
    AddressRange ipcBuffer;

    // The heap that IOS keeps for itself (0x80003148 and 0x8000314C). The old kernels do not publish it.
    std::optional<AddressRange> iosHeap;

    bool operator==(const KernelMemoryAddresses&) const = default;
};

// The commands of the title service (/dev/es) that an IOS answers: the numbers from 1 to the highest one, except the refused ones.
// The title service answers a refused command with the error -1017, like a number that is too high.
struct EsCommandSet
{
    int highestCommand;
    std::vector<int> refusedCommands;

    bool Includes(int command) const;
};

// What a running IOS offers: its number and version and everything in which the working IOS versions differ.
// The services read it and never look at an IOS number. docs/audit/Ios.md says which service reads which feature.
struct IosProfile
{
    IosNumber number = 0;
    std::uint16_t titleVersion = 0;

    // The kernel (Boot). The build date is the word the kernel stores: month, day and year in BCD.
    std::uint32_t kernelBuildDate = 0;
    MemoryPlan memoryPlan = MemoryPlan::Standard;
    bool kernelAppliesTmdIpcRights = false;

    // The crypto core.
    bool cryptoHasHmacDevice = false;

    // The title service.
    EsCommandLevel esCommandLevel = EsCommandLevel::Original;
    bool esReadsVersionOneTickets = false;
    bool esCreatesWfsFolder = false;

    // The file system.
    FileSystemFlavor fileSystemFlavor = FileSystemFlavor::Fs;
    bool fileSystemChecksNameLengthInLookUp = false;
    bool fileSystemRecreatesTmpAtEveryStart = false;

    // The disc drive and the SD card.
    bool discDriveHasNoDiscCommands = false;
    bool sdSupportsHighCapacityCards = false;

    // USB and the keyboard.
    UsbHostGeneration usbHostGeneration = UsbHostGeneration::RootPortsOnly;
    KeyboardDriver keyboardDriver = KeyboardDriver::None;

    // The network.
    NetworkConfigurationKind networkConfiguration = NetworkConfigurationKind::Original;

    // The word the kernel reports as the version of IOS (0x80003140): the number in the upper half, the title version in the lower.
    std::uint32_t VersionWord() const;

    // The memory addresses that the kernel publishes. They follow from the memory plan.
    KernelMemoryAddresses MemoryAddresses() const;

    // The commands of the title service. They follow from the command level.
    EsCommandSet EsCommands() const;

    // The file system has the device /dev/flash for raw access to the NAND. Only the flavors without P have it.
    bool HasFlashDevice() const;

    bool operator==(const IosProfile&) const = default;
};

// Makes the profile of one IOS: its version from the list, and one pass over the feature table.
IosProfile MakeProfile(const IosVersion& version, const IosFeatureTable& features);

}
