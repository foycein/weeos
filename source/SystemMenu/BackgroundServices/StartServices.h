#pragma once

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Title/TitleService.h"

namespace SystemMenu
{

// The work on the NAND that the menu does at every start, without a screen, in the order of the original: the boot stage
// prepares its boot cache (PrepareBootCache), the system constructor deletes the titles of homebrew programs right after the start
// of the title library, and later the empty titles while the backup file of the title metadata is missing (TitleCleanup).
// The program calls it when the menu has started. The title service must run the System Menu.
void RunStartServices(Ios::FileSystemService& fileSystem, Ios::TitleService& titles);

}
