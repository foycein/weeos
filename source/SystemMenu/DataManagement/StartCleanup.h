#pragma once

#include "SystemMenu/DataManagement/DataItems.h"
#include "SystemMenu/DataManagement/DataManagementServices.h"

namespace SystemMenu
{

// The clean passes of the worker when an editor of Data Management starts (0x81349E74). They repair what a power failure in the
// middle of a NAND write leaves behind, before the lists are read:
//
// * Every editor: a channel whose code ends with 'A' (the copy for every region) goes, with its ticket and its save data, when the
//   same channel of a region (the same code but the last letter) is on the NAND with contents too (0x8134A380).
// * The channels: a channel without any content goes when nothing else of it is left; a channel whose contents are only partly
//   there loses them, so that it can come back whole from the SD card (0x8134A588).
// * The save data: a data folder that has files but no banner.bin is erased, unless it holds the folder noerase (0x8134A718).
//
// The data of games, which the original deletes in the same pass, does not occur in WeeOS (docs/audit/OpenPoints/SystemMenu.md SYS-71).
void CleanUpBeforeTheList(DataManagementServices& services, DataKind kind);

}
