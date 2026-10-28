#pragma once

#include <cstdint>
#include <mutex>

namespace Ios
{

// What the drive says about itself when it is asked (the Inquiry command).
struct DriveInformation
{
    std::uint16_t revisionLevel;
    std::uint16_t deviceCode;
    std::uint32_t releaseDate;
    std::uint8_t version;
};

// Whether a disc is in the slot. On the console the cover register tells: its first bit is set while no disc is inside.
// The SDK reads that as a cover that is not closed, and the System Menu shows "No Disk".
enum class CoverState
{
    DiscPresent,
    NoDisc,
};

// The error codes of the drive itself. One code holds a SCSI sense key, an additional sense code and a qualifier, one byte each.
enum class DriveErrorCode : std::uint32_t
{
    None = 0,

    // Sense key 2 (not ready), additional sense code 0x3A (medium not present). The SDK treats this error as "no disc".
    MediumNotPresent = 0x023A00,
};

// The answer to a command of the drive. The numbers are the result codes that /dev/di returns.
enum class DriveReply : std::uint8_t
{
    Success = 1,

    // The drive reported an error. RequestError tells which one.
    DriveError = 2,

    // The command asked for something the drive module never allows.
    SecurityError = 0x20,
};

// The disc drive as it is with no disc in the slot, in place of /dev/di.
//
// A disc source that fills the slot does not exist yet, because games are not part of the system for now.
// So every command that needs a disc ends the way it ends on the console when the slot is empty:
// the drive reports that no medium is present. The commands that only work with a disc are not built.
//
// Thread safe.
class DiscDriveService
{
public:
    // The drive's description. These are the numbers of the drive of the reference console, which the System Menu keeps in its cache file.
    DriveInformation Inquire() const;

    // Resets the drive. The drive forgets its last error.
    void Reset();

    CoverState GetCoverState() const;

    // Clears the cover interrupt of the drive. There is no interrupt here, and the drive module answers Success whatever the state is.
    DriveReply ClearCoverInterrupt();

    // Reads the id of the disc. Always fails with an empty slot: DriveError, and RequestError gives MediumNotPresent.
    DriveReply ReadDiscId();

    // Reads from the system area of a disc. The position and the length are in bytes. The drive module only allows
    // three places: the first 0x50000 bytes, and 0x20 bytes at 0x118280000 and at 0x1FB500000. Anything else is a SecurityError.
    // An allowed place fails like ReadDiscId with an empty slot.
    DriveReply ReadSystemArea(std::uint64_t position, std::uint32_t length);

    // Gives the last error of the drive and then forgets it.
    DriveErrorCode RequestError();

private:
    DriveReply ReportNoMedium();

    mutable std::mutex mutex;
    DriveErrorCode lastError = DriveErrorCode::None;
};

}
