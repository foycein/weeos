#pragma once

#include "SystemMenu/BackgroundServices/NandFileAccess.h"

#include "Formats/TitleMetadataBackupFile.h"
#include "Ios/Title/TitleId.h"
#include "Ios/Title/TitleService.h"

#include <optional>

namespace SystemMenu
{

// The functions of the menu that delete titles and keep their title metadata (ESMisc of the original, iplESMisc.cpp,
// 0x81365704 to 0x81368758). The title metadata of every title the menu deletes goes into the backup file tmds.sys first,
// so that the title can be set up again when it comes back from the SD card.
class TitleCleanup
{
public:
    // The services must stay alive as long as this object. The title service must run the System Menu.
    TitleCleanup(NandFileAccess& files, Ios::TitleService& titles);

    // DeleteTitle (0x81366940): keeps the title metadata in the backup file, unless it is there already, then deletes the title.
    // The ticket stays. A backup that fails does not stop the delete.
    Ios::Result<void> DeleteTitleKeepingBackup(Ios::TitleId titleId);

    // CheckSafeDeleteTitle (0x81366758): true when the title holds nothing any more: no content of its own on the NAND,
    // a version below 256 and an empty data folder.
    bool IsTitleEmpty(Ios::TitleId titleId);

    // DeleteEmptyTitles (0x81366A2C), run by the start of the menu only while the backup file is missing: deletes every empty
    // title outside the system titles, then makes the backup file. So it runs once on a NAND.
    void DeleteEmptyTitlesOnce();

    // PrepareTitleDir (0x813670A0): when the title service does not know the title, its backed-up title metadata is installed again.
    // NotFound when the backup file has none for it.
    Ios::Result<void> RestoreTitleMetadata(Ios::TitleId titleId);

    // DeleteTitleContent (0x81366EE4), when the user deletes a channel: the meta data of the channel goes, then an empty title
    // is deleted with its backup, and the contents of any other title, which keeps its ticket and its data.
    Ios::Result<void> DeleteChannel(Ios::TitleId titleId);

    // A part of InitSavedata (0x813673E0), at every start: five titles that only homebrew programs use are deleted with their tickets.
    void DeleteBlockedTitles();

private:
    bool HasContentFiles(Ios::TitleId titleId);
    int CountOwnContentsOnNand(Ios::TitleId titleId);
    bool IsDataFolderEmpty(Ios::TitleId titleId);
    std::optional<Formats::TitleMetadataBackup> FindBackup(Ios::TitleId titleId);
    Ios::Result<void> AddBackup(Ios::TitleId titleId);
    void DeleteWithTickets(Ios::TitleId titleId);

    NandFileAccess& files;
    Ios::TitleService& titles;
};

}
