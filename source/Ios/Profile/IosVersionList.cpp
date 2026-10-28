#include "Ios/Profile/IosVersionList.h"

#include <algorithm>
#include <utility>

namespace Ios
{

IosVersionList::IosVersionList(std::vector<IosVersion> versionsOfConsole)
    : versions(std::move(versionsOfConsole))
{
}

const std::vector<IosVersion>& IosVersionList::Versions() const
{
    return versions;
}

const IosVersion* IosVersionList::Find(IosNumber number) const
{
    auto found = std::find_if(versions.begin(), versions.end(),
        [number](const IosVersion& version) { return version.number == number; });

    if (found == versions.end())
        return nullptr;

    return &*found;
}

}
