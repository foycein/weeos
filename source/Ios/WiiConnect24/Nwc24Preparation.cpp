#include "Ios/WiiConnect24/Nwc24Preparation.h"

#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NewEntryDefaults.h"
#include "Ios/WiiConnect24/MailboxVolume.h"
#include "Ios/WiiConnect24/Nwc24Files.h"

#include <optional>
#include <string_view>

namespace Ios
{

namespace
{

constexpr Caller SystemMenuCaller = {SystemMenuUserId, SystemMenuGroupId};

constexpr std::string_view FolderPath = "/shared2/wc24";
constexpr std::string_view MailboxFolderPath = "/shared2/wc24/mbox";
constexpr std::string_view MessageConfigPath = "/shared2/wc24/nwc24msg.cfg";
constexpr std::string_view MessageConfigBackupPath = "/shared2/wc24/nwc24msg.cbk";
constexpr std::string_view FriendListPath = "/shared2/wc24/nwc24fl.bin";
constexpr std::string_view FriendRecordListPath = "/shared2/wc24/nwc24fls.bin";
constexpr std::string_view DownloadListPath = "/shared2/wc24/nwc24dl.bin";
constexpr std::string_view SendControlPath = "/shared2/wc24/mbox/wc24send.ctl";
constexpr std::string_view ReceiveControlPath = "/shared2/wc24/mbox/wc24recv.ctl";
constexpr std::string_view SendVolumePath = "/shared2/wc24/mbox/wc24send.mbx";
constexpr std::string_view ReceiveVolumePath = "/shared2/wc24/mbox/wc24recv.mbx";
constexpr std::string_view ScratchFilePaths[] = {"/shared2/wc24/mbox/recvtmp.msg", "/shared2/wc24/mbox/dlcnt.bin"};

// The first bytes of a volume say whether it is one.
constexpr std::size_t VolumeHeadSize = 16;

NandPath PathOf(std::string_view text)
{
    return NandPath::Parse(text).Value();
}

bool Exists(const FileSystemService& fileSystem, const NandPath& path)
{
    return fileSystem.GetAttributes(path, SystemMenuCaller).Succeeded();
}

Result<void> MakeFolderIfMissing(FileSystemService& fileSystem, std::string_view folderPath)
{
    NandPath folder = PathOf(folderPath);
    if (Exists(fileSystem, folder))
        return Result<void>();

    return fileSystem.CreateDirectory(folder, ReadWriteForEveryone, NoEntryFlags, SystemMenuCaller);
}

// The file as it is on the NAND: its length and its first bytes. Nothing when it is missing or cannot be read.
struct FileHead
{
    std::uint64_t length;
    Formats::Bytes firstBytes;
};

std::optional<FileHead> ReadFileHead(FileSystemService& fileSystem, std::string_view path, std::size_t headSize)
{
    Result<OpenFile> opened = fileSystem.Open(PathOf(path), OpenMode::Read, SystemMenuCaller);
    if (opened.Failed())
        return std::nullopt;

    Result<FileStatistics> statistics = opened.Value().GetStatistics();
    Result<Formats::Bytes> bytes = opened.Value().Read(headSize);
    if (statistics.Failed() || bytes.Failed())
        return std::nullopt;

    return FileHead{statistics.Value().length, std::move(bytes.Value())};
}

// The whole file, when it is not longer than the limit. Nothing when it is missing, longer or cannot be read.
std::optional<Formats::Bytes> ReadSmallFile(FileSystemService& fileSystem, std::string_view path, std::size_t limit)
{
    std::optional<FileHead> head = ReadFileHead(fileSystem, path, limit + 1);
    if (!head || head->length > limit)
        return std::nullopt;

    return std::move(head->firstBytes);
}

// Puts the bytes into the file. A file that is there is deleted first, because it may have the wrong length.
Result<void> ReplaceFile(FileSystemService& fileSystem, std::string_view path, Formats::ByteView bytes)
{
    NandPath nandPath = PathOf(path);
    if (Exists(fileSystem, nandPath))
    {
        Result<void> deleted = fileSystem.DeleteEntry(nandPath, SystemMenuCaller);
        if (deleted.Failed())
            return deleted;
    }

    Result<void> created = fileSystem.CreateFile(nandPath, ReadWriteForEveryone, NoEntryFlags, SystemMenuCaller);
    if (created.Failed())
        return created;

    Result<OpenFile> opened = fileSystem.Open(nandPath, OpenMode::Write, SystemMenuCaller);
    if (opened.Failed())
        return opened.Error();

    Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        return written.Error();

    return Result<void>();
}

Result<void> PrepareMessageConfig(FileSystemService& fileSystem)
{
    std::optional<Formats::Bytes> config = ReadSmallFile(fileSystem, MessageConfigPath, MessageConfigSize);
    std::optional<Formats::Bytes> backup = ReadSmallFile(fileSystem, MessageConfigBackupPath, MessageConfigSize);
    bool configIsRight = config && IsMessageConfig(*config);
    bool backupIsRight = backup && IsMessageConfig(*backup);

    Formats::Bytes right = MakeMessageConfig();
    if (configIsRight)
        right = *config;
    else if (backupIsRight)
        right = *backup;

    if (!configIsRight)
    {
        Result<void> replaced = ReplaceFile(fileSystem, MessageConfigPath, right);
        if (replaced.Failed())
            return replaced;
    }

    if (!backupIsRight)
        return ReplaceFile(fileSystem, MessageConfigBackupPath, right);

    return Result<void>();
}

struct ListFile
{
    std::string_view path;
    std::uint32_t magic;
    std::size_t size;
    Formats::Bytes (*MakeNew)();
};

constexpr ListFile ListFiles[] =
{
    {FriendListPath, FriendListMagic, FriendListSize, MakeFriendList},
    {FriendRecordListPath, FriendRecordListMagic, FriendRecordListSize, MakeFriendRecordList},
    {DownloadListPath, DownloadListMagic, DownloadListSize, MakeDownloadList},
};

Result<void> PrepareLists(FileSystemService& fileSystem)
{
    for (const ListFile& list : ListFiles)
    {
        std::optional<Formats::Bytes> bytes = ReadSmallFile(fileSystem, list.path, list.size);
        if (bytes && IsList(*bytes, list.magic, list.size))
            continue;

        Result<void> replaced = ReplaceFile(fileSystem, list.path, list.MakeNew());
        if (replaced.Failed())
            return replaced;
    }
    return Result<void>();
}

struct MailboxFiles
{
    Mailbox mailbox;
    std::string_view controlPath;
    std::string_view volumePath;
};

constexpr MailboxFiles MailboxFileList[] =
{
    {Mailbox::Send, SendControlPath, SendVolumePath},
    {Mailbox::Receive, ReceiveControlPath, ReceiveVolumePath},
};

Result<void> PrepareMailbox(FileSystemService& fileSystem, const Clock& clock, const MailboxFiles& files)
{
    std::optional<Formats::Bytes> control = ReadSmallFile(fileSystem, files.controlPath, MailboxControlSize(files.mailbox));
    bool controlIsRight = control && IsMailboxControl(*control, files.mailbox);

    std::optional<FileHead> volume = ReadFileHead(fileSystem, files.volumePath, VolumeHeadSize);
    bool volumeIsRight = volume && IsMailboxVolume(volume->firstBytes, volume->length, files.mailbox);

    if (!controlIsRight)
    {
        Result<void> replaced = ReplaceFile(fileSystem, files.controlPath, MakeMailboxControl(files.mailbox));
        if (replaced.Failed())
            return replaced;
    }

    if (controlIsRight && volumeIsRight)
        return Result<void>();

    return ReplaceFile(fileSystem, files.volumePath, MakeMailboxVolume(files.mailbox, MakeFatTimestamp(clock.Now())));
}

void DeleteScratchFiles(FileSystemService& fileSystem)
{
    for (std::string_view path : ScratchFilePaths)
    {
        NandPath nandPath = PathOf(path);
        if (Exists(fileSystem, nandPath))
            fileSystem.DeleteEntry(nandPath, SystemMenuCaller);
    }
}

}

Result<void> PrepareWiiConnect24Files(FileSystemService& fileSystem, const Clock& clock)
{
    for (std::string_view folder : {FolderPath, MailboxFolderPath})
    {
        Result<void> made = MakeFolderIfMissing(fileSystem, folder);
        if (made.Failed())
            return made;
    }

    Result<void> configPrepared = PrepareMessageConfig(fileSystem);
    if (configPrepared.Failed())
        return configPrepared;

    Result<void> listsPrepared = PrepareLists(fileSystem);
    if (listsPrepared.Failed())
        return listsPrepared;

    for (const MailboxFiles& files : MailboxFileList)
    {
        Result<void> prepared = PrepareMailbox(fileSystem, clock, files);
        if (prepared.Failed())
            return prepared;
    }

    DeleteScratchFiles(fileSystem);
    return Result<void>();
}

}
