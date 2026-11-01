#include "SystemMenu/WiiMenu/IosMenuConsole.h"

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/BackgroundServices/PlayTimeLog.h"
#include "SystemMenu/BackgroundServices/SharedFlagsFile.h"
#include "SystemMenu/MenuError.h"

#include "Ios/Title/ContentTypes.h"

#include <algorithm>
#include <format>
#include <utility>

namespace SystemMenu
{

namespace
{

// The banner is the first content of a channel title.
constexpr std::uint16_t BannerContentIndex = 0;

// The menu keeps its files private: the owner reads and writes them, nobody else does.
constexpr Ios::EntryPermissions MenuFilePermissions = {Ios::AccessRights::ReadWrite, Ios::AccessRights::None, Ios::AccessRights::None};
constexpr std::uint8_t NoFileFlags = 0;

[[noreturn]] void FailWith(const std::string& problem, Ios::IosError error)
{
    throw MenuError(std::format("{} (result code {})", problem, Ios::OriginalResultCode(error)));
}

Ios::NandPath ParseNandPath(std::string_view path)
{
    Ios::Result<Ios::NandPath> parsed = Ios::NandPath::Parse(path);
    if (parsed.Failed())
        FailWith(std::format("The NAND has no path {}", path), parsed.Error());

    return parsed.Value();
}

// A content that the title needs is on the NAND, or it is optional.
bool AreAllContentsPresent(const Ios::TitleMetadataView& view, const std::vector<std::uint32_t>& presentContentIds)
{
    for (const Ios::ContentView& content : view.contents)
    {
        bool isOptional = (content.type & Ios::OptionalContentTypeFlag) != 0;
        bool isPresent = std::find(presentContentIds.begin(), presentContentIds.end(), content.id) != presentContentIds.end();
        if (!isPresent && !isOptional)
            return false;
    }
    return true;
}

}

IosMenuConsole::IosMenuConsole(Ios::IosServices& iosServices, Settings::PcClock clockOfThePc)
    : titles(iosServices.GetTitles())
    , fileSystem(iosServices.GetFileSystem())
    , pcClock(std::move(clockOfThePc))
{
}

std::vector<InstalledTitle> IosMenuConsole::ListInstalledTitles()
{
    Ios::Result<std::vector<Ios::TitleId>> titleIds = titles.ListInstalledTitles();
    if (titleIds.Failed())
        FailWith("The title service cannot list the titles", titleIds.Error());

    std::vector<InstalledTitle> installedTitles;
    for (Ios::TitleId titleId : titleIds.Value())
        installedTitles.push_back(DescribeTitle(titleId));

    return installedTitles;
}

InstalledTitle IosMenuConsole::DescribeTitle(Ios::TitleId titleId)
{
    InstalledTitle installedTitle;
    installedTitle.id = titleId;

    Ios::Result<Ios::TitleMetadataView> view = titles.ReadTitleMetadataView(titleId);
    Ios::Result<std::vector<Ios::TicketView>> tickets = titles.ListTicketViews(titleId);
    Ios::Result<std::vector<std::uint32_t>> presentContentIds = titles.ListPresentContentIds(titleId);
    if (view.Failed() || tickets.Failed() || presentContentIds.Failed())
        return installedTitle;

    installedTitle.version = view.Value().titleVersion;
    installedTitle.groupId = view.Value().groupId;
    for (const Ios::ContentView& content : view.Value().contents)
    {
        if (content.index == BannerContentIndex)
            installedTitle.bannerSize = content.size;
    }

    bool hasTicket = !tickets.Value().empty();
    installedTitle.isInstalledProperly = hasTicket && AreAllContentsPresent(view.Value(), presentContentIds.Value());
    return installedTitle;
}

Formats::Bytes IosMenuConsole::ReadBannerContent(Ios::TitleId titleId)
{
    Ios::Result<std::vector<Ios::TicketView>> tickets = titles.ListTicketViews(titleId);
    if (tickets.Failed() || tickets.Value().empty())
        throw MenuError(std::format("The title {} has no ticket, so its banner cannot be opened", titleId.ToString()));

    Ios::Result<Ios::ContentStream> stream = titles.OpenContent(titleId, tickets.Value().front(), BannerContentIndex);
    if (stream.Failed())
        FailWith(std::format("The banner of the title {} cannot be opened", titleId.ToString()), stream.Error());

    return stream.Value().Read(stream.Value().GetLength());
}

std::optional<Formats::Bytes> IosMenuConsole::ReadNandFile(std::string_view path)
{
    Ios::NandPath nandPath = ParseNandPath(path);
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(nandPath, Ios::OpenMode::Read, titles.GetCaller());
    if (opened.Failed() && opened.Error() == Ios::IosError::NotFound)
        return std::nullopt;

    if (opened.Failed())
        FailWith(std::format("The file {} cannot be opened", path), opened.Error());

    Ios::Result<Ios::FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        FailWith(std::format("The size of the file {} cannot be read", path), statistics.Error());

    Ios::Result<Formats::Bytes> bytes = opened.Value().Read(statistics.Value().length);
    if (bytes.Failed())
        FailWith(std::format("The file {} cannot be read", path), bytes.Error());

    return bytes.Value();
}

void IosMenuConsole::WriteNandFile(std::string_view path, Formats::ByteView bytes)
{
    Ios::NandPath nandPath = ParseNandPath(path);
    Ios::Caller caller = titles.GetCaller();

    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(nandPath, Ios::OpenMode::ReadWrite, caller);
    if (opened.Failed() && opened.Error() == Ios::IosError::NotFound)
        opened = MakeFileAndOpenIt(nandPath, caller);

    if (opened.Failed())
        FailWith(std::format("The file {} cannot be opened for writing", path), opened.Error());

    Ios::Result<Ios::FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        FailWith(std::format("The size of the file {} cannot be read", path), statistics.Error());

    if (statistics.Value().length != 0 && statistics.Value().length != bytes.size())
        throw MenuError(std::format("The file {} has {} bytes and cannot be replaced by {} bytes", path, statistics.Value().length, bytes.size()));

    Ios::Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        FailWith(std::format("The file {} cannot be written", path), written.Error());
}

Ios::Result<Ios::OpenFile> IosMenuConsole::MakeFileAndOpenIt(const Ios::NandPath& path, const Ios::Caller& caller)
{
    Ios::Result<void> created = fileSystem.CreateFile(path, MenuFilePermissions, NoFileFlags, caller);
    if (created.Failed())
        FailWith(std::format("The file {} cannot be made", path.ToString()), created.Error());

    return fileSystem.Open(path, Ios::OpenMode::ReadWrite, caller);
}

void IosMenuConsole::WritePlayRecord(const Formats::PlayRecord& record)
{
    NandFileAccess files(fileSystem, titles);
    SystemMenu::WritePlayRecord(files, record);
}

LocalTime IosMenuConsole::GetLocalTime()
{
    Settings::CalendarTime now = Settings::ConsoleClock::ToCalendarTime(pcClock());

    LocalTime time;
    time.year = now.year;
    time.month = now.month;
    time.day = now.day;
    time.hour = now.hour;
    time.minute = now.minute;
    time.second = now.second;
    return time;
}

void IosMenuConsole::UpdateSharedFlags()
{
    NandFileAccess files(fileSystem, titles);
    UpdateSharedFlagsFile(files);
}

}
