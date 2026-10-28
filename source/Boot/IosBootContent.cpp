#include "Boot/IosBootContent.h"

#include "Boot/BootError.h"
#include "Ios/Caller.h"
#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/KernelFiles.h"
#include "Ios/Title/SharedContentMap.h"
#include "Ios/Title/TitlePaths.h"
#include "Ios/Title/UserIdTable.h"

#include <format>
#include <string>

namespace Boot
{

namespace
{

// The kernel call that starts a new kernel refuses a file that is longer than this (11 MiB).
constexpr std::uint32_t LargestKernelFileSize = 0xB00000;

// The version word keeps the IOS number, the low byte of the title id, in its upper half.
constexpr std::uint32_t IosNumberMask = 0xFF;
constexpr int BitsPerHalfWord = 16;

std::string DescribeIos(Ios::TitleId iosTitleId)
{
    return std::format("IOS{} ({})", iosTitleId.lowPart, iosTitleId.ToString());
}

void RequireTicketOfItsOwn(Ios::TitleService& titles, Ios::TitleId iosTitleId)
{
    Ios::Result<std::vector<Ios::TicketView>> ticketViews = titles.ListTicketViews(iosTitleId);
    if (ticketViews.Failed())
        throw BootError(std::format("The ticket file of {} cannot be read", DescribeIos(iosTitleId)), ticketViews.Error());

    if (ticketViews.Value().empty())
        throw BootError(std::format("The NAND has no ticket for {}", DescribeIos(iosTitleId)));

    if (ticketViews.Value().front().titleId != iosTitleId)
    {
        throw BootError(std::format("The first ticket for {} belongs to title {}", DescribeIos(iosTitleId),
            ticketViews.Value().front().titleId.ToString()), Ios::IosError::TicketInvalid);
    }
}

Formats::TitleMetadata ReadIosTitleMetadata(Ios::TitleService& titles, Ios::TitleId iosTitleId)
{
    Ios::Result<Formats::TitleMetadata> titleMetadata = titles.ReadTitleMetadata(iosTitleId);
    if (titleMetadata.Failed() && titleMetadata.Error() == Ios::IosError::NotFound)
        throw BootError(std::format("{} is not installed", DescribeIos(iosTitleId)), Ios::IosError::RequiredIosNotInstalled);

    if (titleMetadata.Failed())
        throw BootError(std::format("The title metadata of {} cannot be read", DescribeIos(iosTitleId)), titleMetadata.Error());

    return titleMetadata.Value();
}

// For an IOS the "system version" of the title metadata is not another title. It is the boot2 version the IOS needs,
// with a zero upper half. The console refuses an IOS that needs a newer boot2.
void RequireBootTwoVersion(const Formats::TitleMetadata& titleMetadata, std::uint32_t bootTwoVersion, Ios::TitleId iosTitleId)
{
    Ios::TitleId systemVersion = Ios::TitleId::FromValue(titleMetadata.requiredSystemVersion);
    if (systemVersion.highPart != 0)
    {
        throw BootError(std::format("{} names the system version {} but an IOS names a boot2 version, which has a zero upper half",
            DescribeIos(iosTitleId), systemVersion.ToString()));
    }

    if (systemVersion.lowPart > bootTwoVersion)
    {
        throw BootError(std::format("{} needs boot2 version {} but this console has boot2 version {}",
            DescribeIos(iosTitleId), systemVersion.lowPart, bootTwoVersion));
    }
}

const Formats::ContentRecord& FindBootRecord(const Formats::TitleMetadata& titleMetadata, Ios::TitleId iosTitleId)
{
    if (titleMetadata.bootIndex >= titleMetadata.contents.size())
    {
        throw BootError(std::format("The title metadata of {} lists {} contents but its boot index is {}",
            DescribeIos(iosTitleId), titleMetadata.contents.size(), titleMetadata.bootIndex), Ios::IosError::TitleInvalidArgument);
    }

    return titleMetadata.contents[titleMetadata.bootIndex];
}

Ios::NandPath FindKernelFile(Ios::KernelFiles& files, const Formats::ContentRecord& bootRecord, Ios::TitleId iosTitleId)
{
    if (!Ios::IsSharedContent(bootRecord))
        return Ios::PrivateContentPath(iosTitleId, bootRecord.id);

    Ios::Result<Ios::SharedContentMap> sharedContents = Ios::SharedContentMap::Read(files);
    if (sharedContents.Failed())
        throw BootError("The content map of the NAND cannot be read", sharedContents.Error());

    std::optional<std::string> name = sharedContents.Value().FindName(bootRecord.hash);
    if (!name)
        throw BootError(std::format("The content map of the NAND has no kernel file for {}", DescribeIos(iosTitleId)), Ios::IosError::NotFound);

    return Ios::SharedContentPath(*name);
}

std::uint32_t MeasureKernelFile(Ios::FileSystemService& fileSystem, const Ios::NandPath& kernelFile, Ios::TitleId iosTitleId)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(kernelFile, Ios::OpenMode::Read, Ios::SuperUserCaller);
    if (opened.Failed())
        throw BootError(std::format("The kernel file {} of {} cannot be opened", kernelFile.ToString(), DescribeIos(iosTitleId)), opened.Error());

    Ios::Result<Ios::FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        throw BootError(std::format("The size of the kernel file {} cannot be read", kernelFile.ToString()), statistics.Error());

    if (statistics.Value().length > LargestKernelFileSize)
    {
        throw BootError(std::format("The kernel file {} of {} has {} bytes and the kernel accepts at most {}",
            kernelFile.ToString(), DescribeIos(iosTitleId), statistics.Value().length, LargestKernelFileSize));
    }

    return statistics.Value().length;
}

void GiveUserIdToIos(Ios::KernelFiles& files, Ios::TitleId iosTitleId)
{
    Ios::Result<Ios::UserIdTable> table = Ios::UserIdTable::Read(files);
    if (table.Failed())
        throw BootError("The list of user ids /sys/uid.sys cannot be read", table.Error());

    table.Value().FindOrAdd(iosTitleId);
    if (!table.Value().HasChanged())
        return;

    Ios::Result<void> written = table.Value().Write(files);
    if (written.Failed())
        throw BootError("The list of user ids /sys/uid.sys cannot be written", written.Error());
}

std::uint32_t MakeVersionWord(Ios::TitleId iosTitleId, std::uint16_t titleVersion)
{
    std::uint32_t iosNumber = iosTitleId.lowPart & IosNumberMask;
    return (iosNumber << BitsPerHalfWord) | titleVersion;
}

}

IosBootContent FindIosBootContent(Ios::IosServices& services, Ios::TitleId iosTitleId)
{
    Ios::TitleService& titles = services.GetTitles();
    Ios::FileSystemService& fileSystem = services.GetFileSystem();
    Ios::KernelFiles files(fileSystem);

    RequireTicketOfItsOwn(titles, iosTitleId);
    Formats::TitleMetadata titleMetadata = ReadIosTitleMetadata(titles, iosTitleId);
    RequireBootTwoVersion(titleMetadata, services.GetConsoleIdentity().GetBootTwoVersion(), iosTitleId);

    const Formats::ContentRecord& bootRecord = FindBootRecord(titleMetadata, iosTitleId);

    IosBootContent content;
    content.iosTitleId = iosTitleId;
    content.titleVersion = titleMetadata.titleVersion;
    content.kernelFile = FindKernelFile(files, bootRecord, iosTitleId);
    content.kernelFileSize = MeasureKernelFile(fileSystem, content.kernelFile, iosTitleId);
    content.versionWord = MakeVersionWord(iosTitleId, titleMetadata.titleVersion);

    GiveUserIdToIos(files, iosTitleId);
    return content;
}

}
