#pragma once

#include "Ios/Profile/IosNumber.h"

#include <cstdint>
#include <vector>

namespace Ios
{

// A stub has a kernel, the title service and the file system and nothing else: no disc drive, no USB, no SD card, no network.
// No title can do anything on a stub. A working IOS has all its modules.
enum class IosKind
{
    Stub,
    Working,
};

// Nintendo signed the title of a stock IOS. A patched IOS was changed afterwards, so its signature does not verify.
enum class IosOrigin
{
    Stock,
    Patched,
};

// The title version that seven stubs carry as a placeholder.
constexpr std::uint16_t PlaceholderTitleVersion = 0xFF00;

// One IOS title.
struct IosVersion
{
    IosNumber number;
    std::uint16_t titleVersion;
    IosKind kind;
    IosOrigin origin;

    // IOS titles that carry exactly the same code form a group. The group is named by its lowest IOS number:
    // IOS34 is in the group of IOS33, and IOS33 is in the group of IOS33.
    IosNumber codeGroup;
};

// The IOS titles that are installed on a console, in the order of their numbers.
class IosVersionList
{
public:
    explicit IosVersionList(std::vector<IosVersion> versionsOfConsole);

    // The 47 IOS titles of the reference console (assets/console-dump), written by hand from docs/research/IosVersions.md.
    static const IosVersionList& OfReferenceConsole();

    const std::vector<IosVersion>& Versions() const;

    // The entry of an IOS, or nothing when the console has no such IOS title.
    const IosVersion* Find(IosNumber number) const;

private:
    std::vector<IosVersion> versions;
};

}
