#include "Ios/Profile/IosFeatureTable.h"

namespace Ios
{

namespace
{

// The build dates of the kernels, as the kernel stores them: month, day and year in BCD.
constexpr std::uint32_t KernelOfMarch1st2010 = 0x00030110;
constexpr std::uint32_t KernelOfSeptember27th2011 = 0x00092711;
constexpr std::uint32_t KernelOfFebruary27th2012 = 0x00022712;

// Every row says which IOS versions have a feature, or have it with one value.
// Only working IOS are listed: a stub has no profile. A fact that every working IOS shares is not a feature.
// IOS31 is the IOS that the console runs after a homebrew installer patched it. Its rows are those of the stock IOS31:
// the places the installer changed (docs/research/IosVersions.md, 5.9) are no features.
//
// tests/IosProfileDumpTest.cpp finds every row again in the dump:
// the build date and the memory plan in the data of the kernel, the command level in the command table of ES,
// the flavor in the build tag of the file system, the flags in the texts and patterns of the modules, and so on.
std::vector<FeatureRow> MakeReferenceRows()
{
    // The IOS versions of the old kind: the kernel image holds the kernel and every module in one file.
    const std::vector<IosNumberRange> monolithIos = {9, Through(12, 15), 17, 21, 22};

    // The IOS versions of the new kind: the modules are files of their own.
    const std::vector<IosNumberRange> modularIos =
        {28, 31, Through(33, 38), 41, 43, 45, 46, 48, 53, Through(55, 58), 61, 62, 80};

    // The IOS versions with the newest title service, from 2009 on.
    const std::vector<IosNumberRange> newestIos = {Through(56, 58), 61, 62, 80};

    return
    {
        // The kernel.
        {Feature::KernelBuildDate, KernelOfSeptember27th2011,
            {9, Through(12, 15), 17, 21, 22, 28, Through(33, 38), 41, 43, 45, 46, 48, 53, Through(55, 58), 61, 80}},
        {Feature::KernelBuildDate, KernelOfFebruary27th2012, {62}},
        {Feature::KernelBuildDate, KernelOfMarch1st2010, {31}},

        {Feature::KernelMemoryPlan, MemoryPlan::Monolith, monolithIos},
        {Feature::KernelMemoryPlan, MemoryPlan::Enlarged, {28}},
        {Feature::KernelMemoryPlan, MemoryPlan::Standard,
            {31, Through(33, 38), 41, 43, 45, 46, 48, 53, Through(55, 58), 61, 62, 80}},

        {Feature::KernelAppliesTmdIpcRights, modularIos},

        // The crypto core.
        {Feature::CryptoHasHmacDevice, monolithIos},

        // The title service.
        {Feature::EsCommandLevel, EsCommandLevel::Original, monolithIos},
        {Feature::EsCommandLevel, EsCommandLevel::SharedContents, {28, 31, Through(33, 36), 38}},
        {Feature::EsCommandLevel, EsCommandLevel::ContentDeletion, {37, 41, 43, 45, 46, 48, 53, 55}},
        {Feature::EsCommandLevel, EsCommandLevel::KoreanKeyCheck, newestIos},

        {Feature::EsReadsVersionOneTickets, newestIos},
        {Feature::EsCreatesWfsFolder, newestIos},

        // The file system.
        {Feature::FileSystemFlavor, FileSystemFlavor::Fs, {9, Through(12, 15), 17, 21, 22, 28}},
        {Feature::FileSystemFlavor, FileSystemFlavor::Ffs, {31, Through(33, 36), 38}},
        {Feature::FileSystemFlavor, FileSystemFlavor::Ffsp, {37, 41, 43, 45, 46, 48, 53, Through(55, 58), 61, 62}},
        {Feature::FileSystemFlavor, FileSystemFlavor::Fsp, {80}},

        // The disc drive, the SD card and the power service.
        {Feature::DiscDriveHasNoDiscCommands, modularIos},
        {Feature::SdSupportsHighCapacityCards, {48, Through(56, 58), 61, 62, 80}},
        {Feature::PowerHasWakeupCommand,
            {Through(12, 15), 17, 21, 22, 28, 31, Through(33, 38), 41, 43, 45, 46, 48, 53, Through(55, 58), 61, 62, 80}},

        // USB and the keyboard.
        {Feature::UsbHostGeneration, UsbHostGeneration::RootPortsOnly, {9}},
        {Feature::UsbHostGeneration, UsbHostGeneration::RootHub, {Through(12, 15), 17, 21}},
        {Feature::UsbHostGeneration, UsbHostGeneration::ExternalHubs, {22, 28}},
        {Feature::UsbHostGeneration, UsbHostGeneration::DeviceHooks,
            {31, Through(33, 38), 41, 43, 45, 46, 48, 53, 55, 56, 61, 62, 80}},
        {Feature::UsbHostGeneration, UsbHostGeneration::NewStack, {57, 58}},

        {Feature::KeyboardDriver, KeyboardDriver::None, {9, Through(12, 15), 17, 21, 22, 28}},
        {Feature::KeyboardDriver, KeyboardDriver::Kbd, {31, Through(33, 36), 38, 41, 43, 45, 46, 48, 80}},
        {Feature::KeyboardDriver, KeyboardDriver::HidVersion4, {37, 53, 55, 56, 61, 62}},
        {Feature::KeyboardDriver, KeyboardDriver::HidVersion5, {57, 58}},

        // The network.
        {Feature::NetworkConfiguration, NetworkConfigurationKind::Original, {9}},
        {Feature::NetworkConfiguration, NetworkConfigurationKind::WithProxyChecks,
            {Through(12, 15), 17, 21, 22, 28, 31, Through(33, 38), 41, 43, 45, 46, 48, 53, Through(55, 58), 61, 62, 80}},
    };
}

}

const IosFeatureTable& IosFeatureTable::OfReferenceConsole()
{
    static const IosFeatureTable referenceTable(MakeReferenceRows());
    return referenceTable;
}

}
