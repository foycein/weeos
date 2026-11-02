#pragma once

#include "Ios/Caller.h"
#include "Ios/Profile/IosProfile.h"
#include "Ios/Title/TitleId.h"
#include "Ios/Title/TitleService.h"

#include <cstdint>

namespace Boot
{

// The two memories of every Wii: 24 MiB at 0x80000000 and 64 MiB at 0x90000000.
constexpr std::uint32_t Mem1Size = 0x01800000;
constexpr std::uint32_t Mem2Size = 0x04000000;

// The hardware version word of the console (low memory 0x80003138). The dump does not tell it. The System Menu only separates
// the values 0, 1, 2, 0x10, 0x11 and larger ones, and 0x11 is the one of the first model.
constexpr std::uint32_t HollywoodVersionWord = 0x00000011;

// What boot1 leaves in low memory 0x80003158. Only the kernel reads it. The value is the example of WiiBrew.
constexpr std::uint32_t MemoryVendorCode = 0x0000FF16;

// The clocks that the System Menu stores in 0x800000F8 and 0x800000FC.
constexpr std::uint32_t BusClockHertz = 243'000'000;
constexpr std::uint32_t ProcessorClockHertz = 729'000'000;

// What a program that was just started needs to know, taken from what IOS tells the PowerPC when it starts one.
// The words that a program writes into low memory itself are not here.
struct BootInformation
{
    // The title that was started, the user and the group it runs as, and the IOS it runs on.
    Ios::TitleId titleId;
    std::uint16_t titleVersion;
    Ios::Caller caller;
    Ios::TitleId iosTitleId;

    // The sizes of the two memories (0x80003100 and 0x80003104, 0x80003118 and 0x8000311C).
    std::uint32_t mem1Size;
    std::uint32_t mem2Size;

    // Where the PowerPC may use the second memory, where it and IOS exchange requests and where IOS keeps its heap (0x80003120 to 0x8000314C).
    Ios::KernelMemoryAddresses mem2Addresses;

    std::uint32_t hollywoodVersion;

    // The version of IOS and the build date of its kernel (0x80003140 and 0x80003144).
    std::uint32_t iosVersionWord;
    std::uint32_t iosBuildDate;

    std::uint32_t memoryVendorCode;

    std::uint32_t busClockHertz;
    std::uint32_t processorClockHertz;
};

// The profile is the one of the IOS that runs the title.
BootInformation MakeBootInformation(const Ios::LaunchPlan& plan, std::uint16_t titleVersion, const Ios::IosProfile& profile);

}
