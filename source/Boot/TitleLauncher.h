#pragma once

#include "Boot/BootInformation.h"
#include "Ios/IosFolders.h"
#include "Ios/IosServices.h"
#include "Ios/Profile/IosProfile.h"
#include "Ios/Title/TicketView.h"
#include "Ios/Title/TitleId.h"

namespace Boot
{

// Starts a title the way the console does it, for the System Menu at power on and for every title that a program asks for later
// (docs/research/Boot2.md 4.5, docs/research/BootChain.md 3.4):
//
//  1. The title service checks the title and the ticket view and gives the title a user id.
//  2. The kernel file of the IOS that the title needs is looked up (IosBootContent).
//  3. The request is written to /sys/launch.sys.
//  4. IOS restarts with the profile of the IOS that the title needs, even when it is the IOS that runs now.
//  5. The new IOS reads the request from the file, checks it again and enters the title.
//
// The services must be running. The services and the files that belong to the program that ran before are gone after the restart:
// the program has stopped everything that uses them before it calls.
class TitleLauncher
{
public:
    // The services must outlive this object.
    TitleLauncher(Ios::IosServices& iosServices, Ios::IosFolders iosFolders);

    // Throws a BootError. When it throws in step 1 to 3 nothing has changed and the program goes on.
    // When it throws in step 4 IOS does not run, and when it throws in step 5 IOS runs with no title entered:
    // the program starts another title or ends.
    BootInformation LaunchTitle(Ios::TitleId titleId, const Ios::TicketView& ticketView);

    // True when LaunchTitle got as far as step 4 and ended the IOS that ran, so that the program that ran before cannot go on.
    bool HasStoppedIos() const;

private:
    // The first pass, steps 1 to 3. Gives back the profile of the IOS that must start next.
    Ios::IosProfile RequestLaunch(Ios::TitleId titleId, const Ios::TicketView& ticketView);

    // The second pass, step 5, in the IOS that was started for the profile.
    BootInformation EnterRequestedTitle(const Ios::IosProfile& profile);

    Ios::IosServices& services;
    Ios::IosFolders folders;
    bool hasStoppedIos = false;
};

}
