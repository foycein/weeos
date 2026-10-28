#include "Ios/WiiConnect24/WiiConnect24Service.h"

#include "Formats/ByteReader.h"
#include "Ios/BigEndianBytes.h"
#include "Ios/FileSystem/NewEntryDefaults.h"
#include "Ios/WiiConnect24/Nwc24Preparation.h"

namespace Ios
{

namespace
{

constexpr std::string_view MiscFilePath = "/shared2/wc24/misc.bin";

// misc.bin holds the time difference twice, in front of zeros, as a 64 bit number.
constexpr std::size_t MiscFileSize = 0x400;
constexpr std::size_t FirstDifferenceOffset = 0;
constexpr std::size_t SecondDifferenceOffset = 8;

NandPath MiscFile()
{
    return NandPath::Parse(MiscFilePath).Value();
}

std::chrono::seconds SecondsSince1970(const Clock& clock)
{
    return std::chrono::floor<std::chrono::seconds>(clock.Now().time_since_epoch());
}

}

WiiConnect24Service::WiiConnect24Service(FileSystemService& fileSystemToUse, const Clock& clockToUse)
    : fileSystem(fileSystemToUse)
    , clock(clockToUse)
{
    LoadTimeDifference();
}

// The difference is not there when the file is missing, has not the size of the file, or both copies are zero.
void WiiConnect24Service::LoadTimeDifference()
{
    Result<OpenFile> opened = fileSystem.Open(MiscFile(), OpenMode::Read, WiiConnect24Caller);
    if (opened.Failed())
        return;

    Result<Formats::Bytes> bytes = opened.Value().Read(MiscFileSize);
    if (bytes.Failed() || bytes.Value().size() != MiscFileSize)
        return;

    Formats::ByteReader reader(bytes.Value(), "misc.bin");
    reader.MoveTo(FirstDifferenceOffset);
    std::uint64_t difference = reader.ReadUint64();
    if (difference == 0)
        return;

    timeDifference = std::chrono::seconds(static_cast<std::int64_t>(difference));
}

Result<void> WiiConnect24Service::PrepareFiles()
{
    return PrepareWiiConnect24Files(fileSystem, clock);
}

Result<void> WiiConnect24Service::SuspendScheduler()
{
    std::lock_guard<std::mutex> lock(mutex);
    ++suspendCount;
    return Result<void>();
}

Result<void> WiiConnect24Service::TrySuspendScheduler()
{
    return SuspendScheduler();
}

Result<void> WiiConnect24Service::ResumeScheduler()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (suspendCount > 0)
        --suspendCount;

    return Result<void>();
}

bool WiiConnect24Service::IsSchedulerSuspended() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return suspendCount > 0;
}

std::chrono::seconds WiiConnect24Service::GetUniversalTime() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return SecondsSince1970(clock) + universalTimeOffset;
}

Result<void> WiiConnect24Service::SetUniversalTime(std::chrono::seconds universalTime)
{
    if (universalTime.count() < EarliestUniversalTime || universalTime.count() > LatestUniversalTime)
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(mutex);
    universalTimeOffset = universalTime - SecondsSince1970(clock);
    return Result<void>();
}

Result<void> WiiConnect24Service::SetRtcCounter(std::uint32_t counter)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::chrono::seconds universalTime = SecondsSince1970(clock) + universalTimeOffset;
    std::chrono::seconds difference = universalTime - std::chrono::seconds(counter);

    Result<void> stored = StoreTimeDifference(difference);
    if (stored.Failed())
        return stored;

    timeDifference = difference;
    return Result<void>();
}

Result<std::chrono::seconds> WiiConnect24Service::GetTimeDifference() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!timeDifference)
        return IosError::NotReady;

    return *timeDifference;
}

Result<void> WiiConnect24Service::StoreTimeDifference(std::chrono::seconds difference)
{
    Formats::Bytes bytes(MiscFileSize);
    PutUint64(bytes, FirstDifferenceOffset, static_cast<std::uint64_t>(difference.count()));
    PutUint64(bytes, SecondDifferenceOffset, static_cast<std::uint64_t>(difference.count()));

    NandPath file = MiscFile();
    if (!fileSystem.GetAttributes(file, WiiConnect24Caller).Succeeded())
    {
        Result<void> created = fileSystem.CreateFile(file, ReadWriteForEveryone, NoEntryFlags, WiiConnect24Caller);
        if (created.Failed())
            return created;
    }

    Result<OpenFile> opened = fileSystem.Open(file, OpenMode::Write, WiiConnect24Caller);
    if (opened.Failed())
        return opened.Error();

    Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        return written.Error();

    return Result<void>();
}

}
