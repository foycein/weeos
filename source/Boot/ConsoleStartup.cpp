#include "Boot/ConsoleStartup.h"

#include "Boot/BootSelection.h"
#include "Boot/IosStartup.h"
#include "Boot/TitleLauncher.h"
#include "Ios/Profile/ProfileChoice.h"

#include <stdexcept>
#include <utility>

namespace Boot
{

namespace
{

// boot2 is a kernel with a file system and the title service. The services of IOS are the only way to get those, so boot2 runs on them,
// with the profile of the IOS of the System Menu. The other services stay idle. The profile does not matter to the file system and the title service.
Ios::IosProfile Boot2SessionProfile()
{
    return Ios::SystemMenuProfile();
}

}

ConsoleStartup::ConsoleStartup(Ios::IosFolders iosFolders)
    : folders(std::move(iosFolders))
{
}

ConsoleStartup::~ConsoleStartup()
{
    PowerOff();
}

const BootInformation& ConsoleStartup::PowerOn()
{
    if (IsOn())
        throw std::logic_error("PowerOn was called while the console is on");

    StartIos(services, folders, Boot2SessionProfile());

    try
    {
        BootSelection selection = SelectBootTitle(services.GetTitles());
        TitleLauncher launcher(services, folders);
        bootInformation = launcher.LaunchTitle(Ios::TitleId::FromValue(selection.titleId), selection.ticketView);
    }
    catch (...)
    {
        PowerOff();
        throw;
    }

    return *bootInformation;
}

bool ConsoleStartup::IsOn() const
{
    return services.IsRunning();
}

Ios::IosServices& ConsoleStartup::GetServices()
{
    RequireOn();
    return services;
}

const BootInformation& ConsoleStartup::GetBootInformation() const
{
    if (!bootInformation)
        throw std::logic_error("The boot information was asked for while no title runs");

    return *bootInformation;
}

const BootInformation& ConsoleStartup::LaunchTitle(Ios::TitleId titleId, const Ios::TicketView& ticketView)
{
    RequireOn();

    TitleLauncher launcher(services, folders);
    try
    {
        bootInformation = launcher.LaunchTitle(titleId, ticketView);
    }
    catch (...)
    {
        if (launcher.HasStoppedIos())
            bootInformation.reset();

        throw;
    }

    return *bootInformation;
}

void ConsoleStartup::PowerOff()
{
    services.Stop();
    bootInformation.reset();
}

void ConsoleStartup::RequireOn() const
{
    if (!IsOn())
        throw std::logic_error("The console is off");
}

}
