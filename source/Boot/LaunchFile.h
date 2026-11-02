#pragma once

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Result.h"
#include "Ios/Title/KernelFiles.h"
#include "Ios/Title/TicketView.h"
#include "Ios/Title/TicketViewBytes.h"
#include "Ios/Title/TitleId.h"

#include <cstddef>

namespace Boot
{

// The title to start and the ticket view that proves the program may start it.
struct LaunchRequest
{
    Ios::TitleId titleId;
    Ios::TicketView ticketView;

    bool operator==(const LaunchRequest&) const = default;
};

// /sys/launch.sys holds the request while IOS restarts: the title id (8 bytes) and the ticket view.
// /sys/space.sys holds zeros. It is there so that the launch file always finds room on the NAND:
// the file is deleted before the launch file is written and put back when the launch file is gone.
// The kernel makes these two files in this order.
constexpr std::size_t TitleIdSize = 8;
constexpr std::size_t LaunchFileSize = TitleIdSize + Ios::TicketViewSize;
constexpr std::size_t SpaceFileSize = 0x4AC4;

class LaunchFile
{
public:
    // The file system must outlive this object.
    explicit LaunchFile(Ios::FileSystemService& fileSystem);

    // Gives up space.sys and writes the launch file. A launch file that is there already is replaced.
    Ios::Result<void> Write(const LaunchRequest& request);

    // NotFound when there is no launch file, ShortRead when the file is not a request.
    Ios::Result<LaunchRequest> Read() const;

    // Reads the request, deletes the launch file and puts space.sys back: what the new IOS does with the file.
    Ios::Result<LaunchRequest> Take();

private:
    Ios::KernelFiles files;
};

}
