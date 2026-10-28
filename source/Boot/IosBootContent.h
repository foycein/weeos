#pragma once

#include "Ios/FileSystem/NandPath.h"
#include "Ios/IosServices.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>

namespace Boot
{

// The kernel file of an IOS, found the way the console finds it before it starts the IOS (docs/research/Boot2.md 4.5).
struct IosBootContent
{
    Ios::TitleId iosTitleId;
    std::uint16_t titleVersion;

    // The file that holds the kernel image: a shared content in /shared1, found through /shared1/content.map, or a private content.
    Ios::NandPath kernelFile;
    std::uint32_t kernelFileSize;

    // What the kernel reports as the version of IOS (low memory 0x80003140): the IOS number in the upper half, the title version in the lower.
    std::uint32_t versionWord;
};

// The kernel file is not read or run. Everything else is done as the console does it:
//  - the IOS has a ticket for its own title,
//  - the IOS needs a boot2 that is not newer than the one of this console,
//  - the file is there and the kernel accepts its size,
//  - the IOS gets its user id in /sys/uid.sys when it has none.
// Throws a BootError that says which of them is wrong. The services must be running.
IosBootContent FindIosBootContent(Ios::IosServices& services, Ios::TitleId iosTitleId);

}
