#include "WeeOS/Console.h"

#include "Boot/BootError.h"
#include "Boot/BootSelection.h"
#include "Boot/TitleIds.h"
#include "Ios/Profile/ProfileChoice.h"

#include <format>
#include <string>
#include <vector>

namespace WeeOS
{

namespace
{

[[noreturn]] void FailWith(const std::string& problem, Ios::IosError error)
{
    throw Boot::BootError(std::format("{} (result code {})", problem, Ios::OriginalResultCode(error)));
}

void StartIos(Ios::IosServices& services, const Ios::IosFolders& folders, std::uint64_t iosTitleId)
{
    Ios::IosNumber iosNumber = static_cast<Ios::IosNumber>(Boot::TitleIdLowPart(iosTitleId));
    Ios::Result<void> started = services.Start(folders, Ios::ChooseProfile(iosNumber));
    if (started.Failed())
    {
        std::string problem = std::format("IOS{} cannot start on the NAND folder {} with the identity folder {}", iosNumber,
            folders.nandFolder.string(), folders.identityFolder.string());
        FailWith(problem, started.Error());
    }
}

// boot2 starts the title with the first ticket of its ticket file.
Ios::TicketView FirstTicketViewOf(const Ios::TitleService& titles, Ios::TitleId titleId)
{
    Ios::Result<std::vector<Ios::TicketView>> views = titles.ListTicketViews(titleId);
    if (views.Failed())
        FailWith(std::format("The tickets of the title {} cannot be read", titleId.ToString()), views.Error());

    if (views.Value().empty())
        throw Boot::BootError(std::format("The title {} has no ticket", titleId.ToString()));

    return views.Value().front();
}

void EnterTitle(Ios::TitleService& titles, Ios::TitleId titleId)
{
    Ios::TicketView ticketView = FirstTicketViewOf(titles, titleId);
    Ios::Result<Ios::LaunchPlan> plan = titles.PrepareLaunch(titleId, ticketView);
    if (plan.Failed())
        FailWith(std::format("The title {} cannot start", titleId.ToString()), plan.Error());

    titles.EnterTitle(plan.Value());
}

}

Console::Console(const Ios::IosFolders& folders)
{
    Boot::BootSelection selection = Boot::SelectBootTitle(folders.nandFolder);
    StartIos(services, folders, selection.iosTitleId);
    EnterTitle(services.GetTitles(), Ios::TitleId::FromValue(selection.titleId));
}

Console::~Console()
{
    services.Stop();
}

Ios::IosServices& Console::Services()
{
    return services;
}

}
