#include "Ios/Profile/ProfileChoice.h"

#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <format>

namespace Ios
{

namespace
{

constexpr IosNumber SystemMenuIosNumber = 80;

constexpr std::uint64_t NoRequiredSystemVersion = 0;

// The IOS that a title runs on: the one its metadata requires, or the title itself when it is an IOS.
IosNumber IosNumberNamedBy(const Formats::TitleMetadata& metadata)
{
    std::uint64_t iosTitleId = metadata.requiredSystemVersion;
    if (iosTitleId == NoRequiredSystemVersion)
        iosTitleId = metadata.titleId;

    TitleId iosTitle = TitleId::FromValue(iosTitleId);
    if (iosTitle.highPart != SystemTitleHighPart)
    {
        std::string message = std::format("The title {:016x} names no IOS: its required system version is {:016x}",
            metadata.titleId, metadata.requiredSystemVersion);
        throw ProfileError(ProfileProblem::NamesNoIos, message);
    }

    return static_cast<IosNumber>(iosTitle.lowPart);
}

}

ProfileError::ProfileError(ProfileProblem problemFound, const std::string& message)
    : std::runtime_error(message)
    , problem(problemFound)
{
}

ProfileProblem ProfileError::Problem() const
{
    return problem;
}

IosProfile ChooseProfile(IosNumber number)
{
    const IosVersion* version = IosVersionList::OfReferenceConsole().Find(number);
    if (version == nullptr)
        throw ProfileError(ProfileProblem::NotInstalled, std::format("IOS{} is not installed on the console", number));

    if (version->kind == IosKind::Stub)
    {
        std::string message = std::format("IOS{} is an empty stub: it has no devices, so no title can run on it", number);
        throw ProfileError(ProfileProblem::EmptyStub, message);
    }

    return MakeProfile(*version, IosFeatureTable::OfReferenceConsole());
}

IosProfile ChooseProfileForTitle(const Formats::TitleMetadata& metadata)
{
    return ChooseProfile(IosNumberNamedBy(metadata));
}

IosProfile SystemMenuProfile()
{
    return ChooseProfile(SystemMenuIosNumber);
}

}
