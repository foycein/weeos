#include "Ios/Profile/IosProfile.h"

#include <algorithm>
#include <stdexcept>

namespace Ios
{

namespace
{

constexpr int VersionWordShift = 16;

// The three memory plans of the kernels.
constexpr std::uint32_t Mem2UsableStart = 0x90000800;

constexpr KernelMemoryAddresses MonolithAddresses =
{
    .mem2End = 0x93400000,
    .mem2Usable = {Mem2UsableStart, 0x933E0000},
    .ipcBuffer = {0x933E0000, 0x93400000},
    .iosHeap = std::nullopt,
};

constexpr KernelMemoryAddresses StandardAddresses =
{
    .mem2End = 0x93600000,
    .mem2Usable = {Mem2UsableStart, 0x935E0000},
    .ipcBuffer = {0x935E0000, 0x93600000},
    .iosHeap = AddressRange{0x93600000, 0x93620000},
};

constexpr KernelMemoryAddresses EnlargedAddresses =
{
    .mem2End = 0x93800000,
    .mem2Usable = {Mem2UsableStart, 0x937E0000},
    .ipcBuffer = {0x937E0000, 0x93800000},
    .iosHeap = AddressRange{0x93800000, 0x93820000},
};

KernelMemoryAddresses AddressesOf(MemoryPlan plan)
{
    switch (plan)
    {
    case MemoryPlan::Monolith: return MonolithAddresses;
    case MemoryPlan::Standard: return StandardAddresses;
    case MemoryPlan::Enlarged: return EnlargedAddresses;
    }

    throw std::logic_error("AddressesOf was asked for a memory plan that does not exist");
}

// The commands of the title service. Command 0 is never valid.
constexpr int FirstEsCommand = 1;
constexpr int LastOriginalCommand = 0x31;
constexpr int SetupStreamKeyCommand = 0x3C;
constexpr int LastSharedContentsCommand = 0x3D;
constexpr int UnusedCommand = 0x3F;
constexpr int LastContentDeletionCommand = 0x40;
constexpr int LastKoreanKeyCheckCommand = 0x45;

EsCommandSet CommandsOf(EsCommandLevel level)
{
    switch (level)
    {
    case EsCommandLevel::Original: return {LastOriginalCommand, {}};
    case EsCommandLevel::SharedContents: return {LastSharedContentsCommand, {SetupStreamKeyCommand}};
    case EsCommandLevel::ContentDeletion: return {LastContentDeletionCommand, {SetupStreamKeyCommand, UnusedCommand}};
    case EsCommandLevel::KoreanKeyCheck: return {LastKoreanKeyCheckCommand, {UnusedCommand}};
    }

    throw std::logic_error("CommandsOf was asked for a command level that does not exist");
}

}

bool EsCommandSet::Includes(int command) const
{
    if (command < FirstEsCommand || command > highestCommand)
        return false;

    return std::find(refusedCommands.begin(), refusedCommands.end(), command) == refusedCommands.end();
}

std::uint32_t IosProfile::VersionWord() const
{
    return (static_cast<std::uint32_t>(number) << VersionWordShift) | titleVersion;
}

KernelMemoryAddresses IosProfile::MemoryAddresses() const
{
    return AddressesOf(memoryPlan);
}

EsCommandSet IosProfile::EsCommands() const
{
    return CommandsOf(esCommandLevel);
}

bool IosProfile::HasFlashDevice() const
{
    return fileSystemFlavor == FileSystemFlavor::Fs || fileSystemFlavor == FileSystemFlavor::Ffs;
}

IosProfile MakeProfile(const IosVersion& version, const IosFeatureTable& features)
{
    IosNumber ios = version.number;

    IosProfile profile;
    profile.number = ios;
    profile.titleVersion = version.titleVersion;

    profile.kernelBuildDate = features.ValueOf(Feature::KernelBuildDate, ios);
    profile.memoryPlan = features.KindOf<MemoryPlan>(Feature::KernelMemoryPlan, ios);
    profile.kernelAppliesTmdIpcRights = features.Has(Feature::KernelAppliesTmdIpcRights, ios);

    profile.cryptoHasHmacDevice = features.Has(Feature::CryptoHasHmacDevice, ios);

    profile.esCommandLevel = features.KindOf<EsCommandLevel>(Feature::EsCommandLevel, ios);
    profile.esReadsVersionOneTickets = features.Has(Feature::EsReadsVersionOneTickets, ios);
    profile.esCreatesWfsFolder = features.Has(Feature::EsCreatesWfsFolder, ios);

    profile.fileSystemFlavor = features.KindOf<FileSystemFlavor>(Feature::FileSystemFlavor, ios);
    profile.fileSystemChecksNameLengthInLookUp = features.Has(Feature::FileSystemChecksNameLengthInLookUp, ios);
    profile.fileSystemRecreatesTmpAtEveryStart = features.Has(Feature::FileSystemRecreatesTmpAtEveryStart, ios);

    profile.discDriveHasNoDiscCommands = features.Has(Feature::DiscDriveHasNoDiscCommands, ios);
    profile.sdSupportsHighCapacityCards = features.Has(Feature::SdSupportsHighCapacityCards, ios);

    profile.usbHostGeneration = features.KindOf<UsbHostGeneration>(Feature::UsbHostGeneration, ios);
    profile.keyboardDriver = features.KindOf<KeyboardDriver>(Feature::KeyboardDriver, ios);

    profile.networkConfiguration = features.KindOf<NetworkConfigurationKind>(Feature::NetworkConfiguration, ios);
    return profile;
}

}
