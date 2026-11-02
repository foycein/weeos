#pragma once

#include <string_view>

namespace Ios
{

// The ways in which the working IOS versions differ from each other. What all of them share is not a feature.
// A feature is a flag (an IOS has it or not), a number, or one of the kinds below.
enum class Feature
{
    // A number: the build date of the kernel, as the kernel stores it. Boot writes it into the low memory block.
    KernelBuildDate,

    // A MemoryPlan.
    KernelMemoryPlan,

    // Flag: the access rights in the title metadata limit which devices a title may open.
    KernelAppliesTmdIpcRights,

    // Flag: the crypto device /dev/hmac exists.
    CryptoHasHmacDevice,

    // An EsCommandLevel.
    EsCommandLevel,

    // Flag: the title service reads tickets of version 1.
    EsReadsVersionOneTickets,

    // Flag: the title service makes the folder /wfs among the standard folders of the NAND.
    EsCreatesWfsFolder,

    // A FileSystemFlavor.
    FileSystemFlavor,

    // Flag: a name of 13 characters or more finds nothing when a path is looked up.
    // The other IOS compare the first 12 characters of the name only.
    FileSystemChecksNameLengthInLookUp,

    // Flag: the file system deletes /tmp and makes it again at every start, also when it is empty.
    // The other IOS do it only when /tmp holds something or is missing.
    FileSystemRecreatesTmpAtEveryStart,

    // Flag: the disc drive answers the commands 0x90 to 0x94, the ones that work with no disc.
    DiscDriveHasNoDiscCommands,

    // Flag: the SD card driver probes for cards with more than 2 GB.
    SdSupportsHighCapacityCards,

    // A UsbHostGeneration.
    UsbHostGeneration,

    // A KeyboardDriver.
    KeyboardDriver,

    // A NetworkConfigurationKind.
    NetworkConfiguration,
};

std::string_view FeatureName(Feature feature);

// How the kernel divides the second memory (MEM2) between the PowerPC and IOS.
// IosProfile::MemoryAddresses gives the addresses of each plan.
enum class MemoryPlan
{
    // The old kernels of IOS9 to IOS22. The PowerPC gets 2 MB less than with the standard plan and the kernel publishes no IOS heap.
    Monolith,

    Standard,

    // IOS28. The PowerPC gets 2 MB more than with the standard plan.
    Enlarged,
};

// Which commands the title service answers. Each level has all the commands of the level before.
// IosProfile::EsCommands gives the commands of each level.
enum class EsCommandLevel
{
    // Titles, tickets, contents, import and export, Encrypt and Decrypt, Sign and VerifySign.
    Original,

    // Adds stored contents and title metadata, shared contents, DiVerifyWithTicketView and DeleteStreamKey.
    SharedContents,

    // Adds DeleteTitleContent and GetV0TicketFromView.
    ContentDeletion,

    // Adds the tickets from views and CheckHasKoreanKey.
    KoreanKeyCheck,
};

// The build of the file system module. The names are the names in the build tag of the module.
// The dump shows no functional difference except one: the flavors without P (FS and FFS) have /dev/flash.
enum class FileSystemFlavor
{
    Fs,
    Ffs,
    Ffsp,
    Fsp,
};

// The generation of the USB host driver /dev/usb/oh0, the one of the external ports.
enum class UsbHostGeneration
{
    // IOS9: devices sit directly at the root ports. No hubs and no isochronous transfers.
    RootPortsOnly,

    // IOS12 to IOS21: a driver for the root hub and isochronous writes.
    RootHub,

    // IOS22 and IOS28: external USB hubs.
    ExternalHubs,

    // IOS31 and later: the driver reports when a device is plugged in or removed.
    DeviceHooks,

    // IOS57 and IOS58: the USB 2.0 stack with its own modules (OHCI0, USB, USB_HUB, USB_VEN).
    NewStack,
};

// The module that serves USB keyboards.
enum class KeyboardDriver
{
    // The old IOS (IOS9 to IOS22 and IOS28) have no keyboard device.
    None,

    // The module KBD, /dev/usb/kbd.
    Kbd,

    // The module USB_HID of 2008, /dev/usb/hid.
    HidVersion4,

    // USB_HID on the new USB stack.
    HidVersion5,
};

// Where the network configuration module NCD keeps the settings, and what it checks in them.
enum class NetworkConfigurationKind
{
    // IOS9: /shared2/sys/net/config.dat. The proxy settings are not checked.
    Original,

    // All later IOS: /shared2/sys/net/02/config.dat. The old file is renamed and the http and ssl proxy settings are checked.
    WithProxyChecks,
};

}
