#include "SystemMenu/FatalError.h"

namespace SystemMenu
{

// The original shows "damaged" for the NAND library's result "corrupt" (-4) and "corrupted files" for every other failure it does not
// handle. Of the errors of WeeOS only a storage failure means that the NAND itself let the menu down.
FatalError FatalErrorOf(Ios::IosError error)
{
    if (error == Ios::IosError::StorageFailure)
        return FatalError::SystemMemoryDamaged;

    return FatalError::SystemFilesCorrupted;
}

}
