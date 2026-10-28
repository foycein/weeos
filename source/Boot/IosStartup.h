#pragma once

#include "Ios/IosFolders.h"
#include "Ios/IosServices.h"
#include "Ios/Profile/IosProfile.h"

namespace Boot
{

// Brings the services of IOS up with the profile of the IOS version that must run.
//
// When IOS runs already it is ended first, as the console does when the kernel of the next IOS takes its place:
// the files that are open are closed and the events that nobody took are gone. The NAND stays as it is.
// The service objects of the old IOS are replaced, so the program that used them has ended before: no thread is inside a service,
// and nothing keeps a reference to one. (The console resets the PowerPC here, so the program that ran is gone.)
// Throws a BootError when the services cannot start (the identity or the NAND folder cannot be used). IOS does not run then.
void StartIos(Ios::IosServices& services, const Ios::IosFolders& folders, const Ios::IosProfile& profile);

}
