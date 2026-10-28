#include "Boot/IosStartup.h"

#include "Boot/BootError.h"

#include <format>

namespace Boot
{

void StartIos(Ios::IosServices& services, const Ios::IosFolders& folders, const Ios::IosProfile& profile)
{
    services.Stop();

    Ios::Result<void> started = services.Start(folders, profile);
    if (started.Failed())
    {
        throw BootError(std::format("IOS{} cannot start with the NAND folder {} and the identity folder {}",
            profile.number, folders.nandFolder.string(), folders.identityFolder.string()), started.Error());
    }
}

}
