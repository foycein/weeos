#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>

namespace Formats
{

constexpr std::size_t StateFileSize = 0x20;
constexpr std::size_t StateFileSpareSize = 24;

// The bits of the flags byte. Every writer of the System Menu sets the first one; the rest of the byte is the cause.
constexpr Byte StateCauseIsRecordedBit = 0x80;

// The boot cache (cache.dat) holds the memory of the menu and the menu may start from it.
constexpr Byte StateBootCacheIsValidBit = 0x40;
constexpr Byte StateCauseMask = 0x3F;

// The causes that the menu writes (the value 2 is only written when it starts the GameCube mode, which WeeOS has not).
constexpr Byte StateCauseTitleLaunched = 1;
constexpr Byte StateCauseShutDownOrReturned = 4;

// What the console was doing when it left the menu. 0xFF is what the menu writes when it starts, so that a power cut while it runs is
// noticed at the next start ("last shutdown sequence is invalid"). Values 4 and 5 belong to the standby of WiiConnect24.
constexpr Byte StateTypeUnknown = 0xFF;
constexpr Byte StateTypeShutDown = 1;
constexpr Byte StateTypeRestart = 2;
constexpr Byte StateTypeReturnToMenu = 3;

// What the drive had: 1 or 2 when the cover is closed (2 unless the menu was started from its boot cache), 3 otherwise.
constexpr Byte StateDiscStateNoDisc = 3;

// Where the menu goes when it starts again: 0 the channels, 1 the Wii Settings, 2 a title that left arguments in NANDBOOTINFO.
constexpr Byte StateReturnToMenu = 0;
constexpr Byte StateReturnToSettings = 1;
constexpr Byte StateReturnWithArguments = 2;

// The file /title/00000001/00000002/data/state.dat: how the console was left, so that the menu knows how to start.
struct StateFile
{
    Byte flags;
    Byte type;
    Byte discState;
    Byte returnTo;

    // Not used by the menu code that was read; kept as they are.
    std::array<Byte, StateFileSpareSize> spare;
};

// The first four bytes are the sum of the seven words that follow. Throws a FormatError when the size or the sum is wrong.
StateFile ReadStateFile(ByteView bytes);

// Writes the sum.
Bytes WriteStateFile(const StateFile& state);

}
