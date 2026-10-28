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

    // The caller has no permission for the call. On the SD card: the card is write protected.
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

    // The SD card. Code of /dev/sdio/slot0.

    // The slot is empty, or the card was taken out. The card driver gives this answer to every command when no card is there.
    NoCardInserted = -10,

    // The time and the sockets of the network family. Codes of /dev/net/kd/time and /dev/net/ip/top.
    // The sockets number their errors in the alphabetical order of the POSIX names: 1 is E2BIG, 6 is EAGAIN, 8 is EBADF, and so on.

    // The value is not ready yet. The WiiConnect24 clock has no time difference before the first time it was set.
    NotReady = -30,

    // The host name is not known. The original gives the same number to a request whose parameters are wrong.
    HostNotFound = -4,

    // Every call that needs a socket fails like this until the socket stack was set up with a network. It is the code of ENETRESET.
    StackNotStarted = -39,

    // The interface option is not known, or the buffer for it is too small. It is the code of ENOPROTOOPT.
    InterfaceOptionNotSupported = -51,

    // The address lookup failed. The original reports it as EAI_AGAIN (-0x12E).
    NameLookupFailed = -302,

    // The secure sockets. Codes of /dev/net/ssl. The module has four contexts and numbers its errors like OpenSSL does:
    // the code is minus the answer of SSL_get_error. A few numbers are the module's own.

    // The SSL library failed, or a limit was reached: no free context, a name or a certificate that is too long,
    // a connection that was never set up. It is SSL_ERROR_SSL.
    SecureLibraryFailure = -1,

    // The socket under the secure connection failed (SSL_ERROR_SYSCALL).
    SecureConnectionFailed = -5,

    // The number is not a context of the module.
    InvalidSecureContext = -8,

    // The module has four root certificates built in and one client certificate.
    BuiltinRootCertificateNotFound = -14,
    BuiltinClientCertificateNotFound = -15,
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
