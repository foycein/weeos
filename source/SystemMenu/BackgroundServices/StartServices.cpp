#include "SystemMenu/BackgroundServices/StartServices.h"

#include "SystemMenu/BackgroundServices/BootCache.h"
#include "SystemMenu/BackgroundServices/TitleCleanup.h"

namespace SystemMenu
{

void RunStartServices(Ios::FileSystemService& fileSystem, Ios::TitleService& titles)
{
    NandFileAccess files(fileSystem, titles);
    PrepareBootCache(files);

    TitleCleanup cleanup(files, titles);
    cleanup.DeleteBlockedTitles();
    cleanup.DeleteEmptyTitlesOnce();
}

}
