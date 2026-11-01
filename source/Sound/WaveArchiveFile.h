#pragma once

#include "Sound/WaveInfo.h"

#include "Formats/Bytes.h"

#include <vector>

namespace Sound
{

// A wave of a wave archive: its description, and the bytes of the small file (RWAV) that holds both the description and the samples.
struct ArchivedWave
{
    // The data location counts from the start of fileBytes.
    WaveInfo info;
    Formats::ByteView fileBytes;
};

// A wave archive (RWAR) holds the waves of a bank, each as a small file of its own. The notes of a bank of version 1.2 name the waves
// by their place in the archive.
struct WaveArchiveFile
{
    std::vector<ArchivedWave> waves;
};

// The result points into the bytes, which must stay alive as long as the result is used.
WaveArchiveFile ReadWaveArchiveFile(Formats::ByteView bytes);

}
