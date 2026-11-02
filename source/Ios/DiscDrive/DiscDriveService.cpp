#include "Ios/DiscDrive/DiscDriveService.h"

namespace Ios
{

namespace
{

struct SystemAreaRange
{
    std::uint64_t start;
    std::uint64_t length;
};

// The places of the disc that the drive module lets a program read without opening a partition.
constexpr SystemAreaRange AllowedSystemAreaRanges[] =
{
    {0, 0x50000},
    {0x118280000, 0x20},
    {0x1FB500000, 0x20},
};

// What the drive of a real console answers to the Inquiry command: revision 0, device code 2, released 2008-12-18, version 0x41.
// The first 0x20 bytes of cache.dat in the System Menu's data folder hold the answer, as the menu stored it.
constexpr DriveInformation ReferenceDriveInformation = {0x0000, 0x0002, 0x20081218, 0x41};

// The module checks that both the start and the end of the read lie inside one range. A range includes its last byte position.
bool IsInsideAllowedRange(std::uint64_t position, std::uint32_t length)
{
    std::uint64_t end = position + length;
    for (const SystemAreaRange& range : AllowedSystemAreaRanges)
    {
        std::uint64_t rangeEnd = range.start + range.length;
        bool startIsInside = position >= range.start && position <= rangeEnd;
        bool endIsInside = end >= range.start && end <= rangeEnd;
        if (startIsInside && endIsInside)
            return true;
    }
    return false;
}

}

DriveInformation DiscDriveService::Inquire() const
{
    return ReferenceDriveInformation;
}

void DiscDriveService::Reset()
{
    std::lock_guard<std::mutex> lock(mutex);
    lastError = DriveErrorCode::None;
}

CoverState DiscDriveService::GetCoverState() const
{
    return CoverState::NoDisc;
}

DriveReply DiscDriveService::ClearCoverInterrupt()
{
    return DriveReply::Success;
}

DriveReply DiscDriveService::ReadDiscId()
{
    std::lock_guard<std::mutex> lock(mutex);
    return ReportNoMedium();
}

DriveReply DiscDriveService::ReadSystemArea(std::uint64_t position, std::uint32_t length)
{
    if (!IsInsideAllowedRange(position, length))
        return DriveReply::SecurityError;

    std::lock_guard<std::mutex> lock(mutex);
    return ReportNoMedium();
}

DriveErrorCode DiscDriveService::RequestError()
{
    std::lock_guard<std::mutex> lock(mutex);
    DriveErrorCode error = lastError;
    lastError = DriveErrorCode::None;
    return error;
}

// The caller holds the mutex.
DriveReply DiscDriveService::ReportNoMedium()
{
    lastError = DriveErrorCode::MediumNotPresent;
    return DriveReply::DriveError;
}

}
