#pragma once

namespace Ios
{

// Why a call to an IOS service failed.
// The number of every enumerator is the result code the original IOS returns for the same cause.
// That way a log line, or a result measured on the console, can be compared with ours directly.
enum class IosError : int
{
    // The file system. Codes of /dev/fs.

    // A malformed path, a name that is too long, or an entry of the wrong kind.
    InvalidArgument = -101,

    // The caller has no permission for the call.
    AccessDenied = -102,

    // The NAND is not a usable file system. The console formats it. We report it.
    NoFileSystem = -104,

    AlreadyExists = -105,
    NotFound = -106,
    NoFreeInodes = -107,
    NoFreeClusters = -108,
    TooManyOpenFiles = -109,
    PathTooDeep = -110,

    // A file that is open cannot be deleted or renamed.
    FileInUse = -111,

    // The NAND chip failed. For us: the PC could not read or write its files.
    StorageFailure = -114,

    // A directory with entries, or a file with data, where an empty one is needed.
    NotEmpty = -115,

    // Every call fails like this after the file system was shut down.
    ShutDown = -119,

    // The title service. Codes of /dev/es.

    // A certificate holds a kind of public key that cannot check the signature.
    PublicKeyTypeInvalid = -1005,

    // A file of the title service holds less than it must, or something that is not what it should be.
    ShortRead = -1009,

    TitleInvalidArgument = -1017,
    DeviceIdMismatch = -1020,
    ContentHashMismatch = -1022,
    AccessRightsIncorrect = -1026,
    IssuerNotFound = -1027,
    TicketNotFound = -1028,
    TicketInvalid = -1029,
    TicketLimitExceeded = -1033,
    HigherVersionInstalled = -1035,
    RequiredIosNotInstalled = -1036,
    ContentCountMismatch = -1037,

    // The crypto core, returned through the title service.

    // A signature did not verify.
    SignatureInvalid = -2011,
};

// The result code the original IOS returns for this error.
inline int OriginalResultCode(IosError error)
{
    return static_cast<int>(error);
}

// The result code the SDK's NAND library gives a program when the file system fails with this error.
// The library translates the codes of /dev/fs with a table. A code that is not in the table becomes -64.
int NandLibraryResultCode(IosError error);

}
