#include "Boot/BootInformation.h"

namespace Boot
{

BootInformation MakeBootInformation(const Ios::LaunchPlan& plan, std::uint16_t titleVersion, const Ios::IosProfile& profile)
{
    BootInformation information;
    information.titleId = plan.titleId;
    information.titleVersion = titleVersion;
    information.caller = plan.caller;
    information.iosTitleId = plan.requiredSystemVersion;
    information.mem1Size = Mem1Size;
    information.mem2Size = Mem2Size;
    information.mem2Addresses = profile.MemoryAddresses();
    information.hollywoodVersion = HollywoodVersionWord;
    information.iosVersionWord = profile.VersionWord();
    information.iosBuildDate = profile.kernelBuildDate;
    information.memoryVendorCode = MemoryVendorCode;
    information.busClockHertz = BusClockHertz;
    information.processorClockHertz = ProcessorClockHertz;
    return information;
}

}
