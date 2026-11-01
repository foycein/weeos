#pragma once

#include "SystemMenu/BackgroundServices/NandFileAccess.h"

#include "Formats/PlayRecordFile.h"

#include <optional>

namespace SystemMenu
{

// The record of the title that is about to start, as the menu writes it just before the start (the operating system call 0x81536DC4,
// made by the channel start window). The first and the last time are both the time of the start.
// A record that cannot be written is left out, as in the original.
void WritePlayRecord(NandFileAccess& files, const Formats::PlayRecord& record);

// The play log (iplPlayTimeLog.cpp, 0x813406CC): the record of the title that ran last, for the Message Board.
// Nothing, and the file stays, when it is missing or damaged. A good record is given back and its file is deleted,
// so that it is counted only once. Throws a MenuError when the file cannot be deleted: the original stops with an error of the NAND.
std::optional<Formats::PlayRecord> TakePlayRecord(NandFileAccess& files);

}
