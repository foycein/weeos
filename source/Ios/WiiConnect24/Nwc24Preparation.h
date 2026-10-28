#pragma once

#include "Ios/Caller.h"
#include "Ios/Clock.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Result.h"

namespace Ios
{

// Makes sure that the folders and files of the WiiConnect24 programs are there and usable, as the SDK's NWC24 library does
// every time the System Menu starts. What is there and right is left as it is. What is missing or wrong is made new:
//
// * the folders /shared2/wc24 and /shared2/wc24/mbox,
// * nwc24msg.cfg and its backup nwc24msg.cbk (one is made from the other when only one is right),
// * nwc24fl.bin, nwc24fls.bin and nwc24dl.bin,
// * the mailboxes wc24send.ctl and wc24recv.ctl with wc24send.mbx and wc24recv.mbx (a new control file takes a new mailbox with it),
// * and recvtmp.msg and dlcnt.bin in the mailbox folder are deleted: they are scratch files of the last session.
//
// Everything belongs to the System Menu, because the menu's library makes it on the console. The new volumes of the mailboxes
// get the time of the clock.
Result<void> PrepareWiiConnect24Files(FileSystemService& fileSystem, const Clock& clock);

}
