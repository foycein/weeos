#pragma once

#include "Ios/Caller.h"
#include "Ios/Clock.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Result.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

namespace Ios
{

// The WiiConnect24 daemon (KD) of IOS runs as this user. It owns the file misc.bin in /shared2/wc24.
constexpr Caller WiiConnect24Caller = {12, 12};

// The earliest and the latest universal time that the daemon accepts, in seconds since 1 January 1970: midnight on 26 December 1995
// and on 2 December 2088.
constexpr std::int64_t EarliestUniversalTime = 0x30DF3B00;
constexpr std::int64_t LatestUniversalTime = 0xDFAEF080;

// The WiiConnect24 daemon (KD) of IOS, in place of /dev/net/kd/request and /dev/net/kd/time.
//
// The daemon downloads data and exchanges mail while the console is on or in standby. It is a scheduler that works in the background.
// The programs that edit its files, for example the Message Board, ask it to stand still first (SuspendScheduler) and let it go on
// afterwards (ResumeScheduler). The console here has no network, so the scheduler has nothing to do and never runs:
// the requests are met at once. The daemon also keeps the time: it knows the universal time and how far it is from the clock of the console.
//
// The requests that need the network (to make or register the account, to download or send now) are not part of this service.
// Nothing can answer them offline, and the programs that make them are not reached without a network.
//
// All calls are thread safe.
class WiiConnect24Service
{
public:
    // Loads the time difference that the daemon stored in misc.bin, if there is one.
    WiiConnect24Service(FileSystemService& fileSystem, const Clock& clock);

    // Makes the folders and files of the WiiConnect24 programs, see PrepareWiiConnect24Files. The System Menu does this when it starts,
    // as its NWC24 library does, so IOS does not do it when it starts. The NAND needs about 9 MiB of room for them.
    Result<void> PrepareFiles();

    // A program asks the scheduler to stand still. Each request has to be paid back with a ResumeScheduler.
    // The original waits until the scheduler stands still (SuspendScheduler) or answers "busy" if it does not (TrySuspendScheduler).
    // Here the scheduler is never busy, so both succeed at once.
    Result<void> SuspendScheduler();
    Result<void> TrySuspendScheduler();

    // Pays back one request. The scheduler goes on when the last one is paid back. A resume without a request does nothing.
    Result<void> ResumeScheduler();

    bool IsSchedulerSuspended() const;

    // The time of day in UTC, in seconds since 1 January 1970. The clock of the PC knows it, so it is ready from the start.
    // The daemon of the console answers "not ready" until it was set, which is a deviation (docs/audit/Ios.md).
    std::chrono::seconds GetUniversalTime() const;

    // Tells the daemon what time it is. From then on its universal time runs on from there.
    // InvalidArgument when the time is not between EarliestUniversalTime and LatestUniversalTime.
    Result<void> SetUniversalTime(std::chrono::seconds universalTime);

    // The clock of the console now shows this number of seconds. The daemon works out how far its universal time is from it
    // and keeps the difference in misc.bin.
    Result<void> SetRtcCounter(std::uint32_t counter);

    // The universal time minus the counter of the clock of the console, as the last SetRtcCounter or the file misc.bin say.
    // NotReady when neither was there.
    Result<std::chrono::seconds> GetTimeDifference() const;

private:
    Result<void> StoreTimeDifference(std::chrono::seconds difference);
    void LoadTimeDifference();

    FileSystemService& fileSystem;
    const Clock& clock;

    mutable std::mutex mutex;
    int suspendCount = 0;
    std::chrono::seconds universalTimeOffset{0};
    std::optional<std::chrono::seconds> timeDifference;
};

}
