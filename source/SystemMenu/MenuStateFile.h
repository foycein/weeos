#pragma once

#include "SystemMenu/NandFiles.h"

#include "Formats/StateFile.h"

#include <optional>

namespace SystemMenu
{

// The file state.dat of the System Menu: how the console left the menu the last time, so that the menu knows how to start.
// Nothing when the file is missing, damaged or cannot be read.
std::optional<Formats::StateFile> ReadMenuState(NandFiles& nand);

// The original goes on when the file cannot be written (0x81537720 gives back a result that no caller looks at), and so does WeeOS.
void WriteMenuState(NandFiles& nand, const Formats::StateFile& state);

// What the menu writes when it starts (main 0x81379D38): the type 0xFF, so that a power cut while the menu runs counts as a cold start.
void RecordMenuStart(NandFiles& nand);

// What the menu writes before it leaves for a reset or a power off (0x8138053C, 0x813808A0, 0x81380600). The place to return to stays
// as the file has it.
void RecordMenuLeft(NandFiles& nand, Formats::Byte stateType);

}
