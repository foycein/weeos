#include "SystemMenu/FatalError.h"

namespace SystemMenu
{

FatalError FatalErrorOf(Ios::IosError error)
{
    switch (error)
    {
    case Ios::IosError::StorageFailure:
    case Ios::IosError::NoFileSystem:
    case Ios::IosError::ShutDown:
        return FatalError::SystemMemoryDamaged;

    case Ios::IosError::DeviceIdMismatch:
    case Ios::IosError::IssuerNotFound:
    case Ios::IosError::SignatureInvalid:
    case Ios::IosError::TicketNotFound:
    case Ios::IosError::TicketInvalid:
        return FatalError::UnauthorizedDevice;

    default:
        return FatalError::SystemFilesCorrupted;
    }
}

}
