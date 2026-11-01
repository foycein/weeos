#include "Boot/TitleLauncher.h"

#include "Boot/BootError.h"
#include "Boot/IosBootContent.h"
#include "Boot/IosStartup.h"
#include "Boot/LaunchFile.h"
#include "Ios/Profile/ProfileChoice.h"

#include <format>
#include <utility>

namespace Boot
{

namespace
{

Formats::TitleMetadata ReadTitleMetadataOrThrow(Ios::TitleService& titles, Ios::TitleId titleId)
{
    Ios::Result<Formats::TitleMetadata> titleMetadata = titles.ReadTitleMetadata(titleId);
    if (titleMetadata.Failed())
        throw BootError(std::format("The title metadata of {} cannot be read", titleId.ToString()), titleMetadata.Error());

    return titleMetadata.Value();
}

Ios::LaunchPlan PrepareLaunchOrThrow(Ios::TitleService& titles, Ios::TitleId titleId, const Ios::TicketView& ticketView)
{
    Ios::Result<Ios::LaunchPlan> plan = titles.PrepareLaunch(titleId, ticketView);
    if (plan.Failed())
        throw BootError(std::format("The title {} cannot start", titleId.ToString()), plan.Error());

    return plan.Value();
}

// The profile of the IOS that the title names. A profile cannot be chosen for a title that names no IOS or an IOS the console has not.
Ios::IosProfile ProfileOfTitle(const Formats::TitleMetadata& titleMetadata, Ios::TitleId titleId)
{
    try
    {
        return Ios::ChooseProfileForTitle(titleMetadata);
    }
    catch (const Ios::ProfileError& error)
    {
        throw BootError(std::format("The title {} cannot start: {}", titleId.ToString(), error.what()));
    }
}

// A version of Nintendo's update server that has the code of the version of the dump, with the build date of its kernel.
struct SameCodeVersion
{
    Ios::IosNumber number;
    std::uint16_t titleVersion;
    std::uint32_t kernelBuildDate;
};

// IOS80 v6944, the one a new WeeOS installs, has the title service, the file system and the modules of the dump's v7200 byte for byte; only
// the build text, the build date of the kernel and two waits for the SEEPROM differ (docs/research/Boot2.md, section 1, row 3).
constexpr SameCodeVersion SameCodeVersions[] = {
    {80, 6944, 0x00030310},
};

// The behavior of IOS is known for the versions of the dump. Another version of the same IOS on the NAND would start with a wrong profile,
// unless it has the same code: then the profile takes its version and its build date.
void AdoptKnownIosVersion(const IosBootContent& kernel, Ios::IosProfile& profile)
{
    if (kernel.versionWord == profile.VersionWord())
        return;

    for (const SameCodeVersion& sameCode : SameCodeVersions)
    {
        if (sameCode.number == profile.number && sameCode.titleVersion == kernel.titleVersion)
        {
            profile.titleVersion = sameCode.titleVersion;
            profile.kernelBuildDate = sameCode.kernelBuildDate;
            return;
        }
    }

    throw BootError(std::format("The NAND has {} version {} but WeeOS knows version {} of it",
        kernel.iosTitleId.ToString(), kernel.titleVersion, profile.titleVersion));
}

}

TitleLauncher::TitleLauncher(Ios::IosServices& iosServices, Ios::IosFolders iosFolders)
    : services(iosServices)
    , folders(std::move(iosFolders))
{
}

BootInformation TitleLauncher::LaunchTitle(Ios::TitleId titleId, const Ios::TicketView& ticketView)
{
    Ios::IosProfile profile = RequestLaunch(titleId, ticketView);

    hasStoppedIos = true;
    StartIos(services, folders, profile);

    return EnterRequestedTitle(profile);
}

bool TitleLauncher::HasStoppedIos() const
{
    return hasStoppedIos;
}

Ios::IosProfile TitleLauncher::RequestLaunch(Ios::TitleId titleId, const Ios::TicketView& ticketView)
{
    Ios::TitleService& titles = services.GetTitles();

    Ios::LaunchPlan plan = PrepareLaunchOrThrow(titles, titleId, ticketView);
    Ios::IosProfile profile = ProfileOfTitle(ReadTitleMetadataOrThrow(titles, titleId), titleId);

    IosBootContent kernel = FindIosBootContent(services, plan.requiredSystemVersion);
    AdoptKnownIosVersion(kernel, profile);

    Ios::Result<void> written = LaunchFile(services.GetFileSystem()).Write(LaunchRequest{titleId, ticketView});
    if (written.Failed())
        throw BootError("The launch file /sys/launch.sys cannot be written", written.Error());

    return profile;
}

BootInformation TitleLauncher::EnterRequestedTitle(const Ios::IosProfile& profile)
{
    Ios::Result<LaunchRequest> request = LaunchFile(services.GetFileSystem()).Take();
    if (request.Failed())
        throw BootError("IOS restarted but the launch file /sys/launch.sys cannot be read", request.Error());

    Ios::TitleService& titles = services.GetTitles();
    Ios::TitleId titleId = request.Value().titleId;

    Ios::LaunchPlan plan = PrepareLaunchOrThrow(titles, titleId, request.Value().ticketView);
    Formats::TitleMetadata titleMetadata = ReadTitleMetadataOrThrow(titles, titleId);

    Ios::Result<void> dataFolder = titles.EnsureDataFolder(titleId);
    if (dataFolder.Failed())
        throw BootError("The data folder of the title cannot be made", dataFolder.Error());

    titles.EnterTitle(plan);

    return MakeBootInformation(plan, titleMetadata.titleVersion, profile);
}

}
