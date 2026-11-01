#pragma once

#include "Sound/NoteParameters.h"
#include "Sound/WaveInfo.h"

#include "Formats/Bytes.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Sound
{

// A note is used for the keys and the velocities (how hard a key is struck) of one region. Both ends belong to the region.
struct NoteRegion
{
    std::uint8_t lowestKey;
    std::uint8_t highestKey;
    std::uint8_t lowestVelocity;
    std::uint8_t highestVelocity;
    NoteParameters note;
};

// An instrument chooses the note to play from the key and the velocity. The files describe this with tables of ranges
// and of single keys. They are turned into a plain list of regions when the file is read.
struct Instrument
{
    std::vector<NoteRegion> regions;
};

// A bank file (RBNK) holds the instruments that sequences play, and the waves they use.
// The samples of the waves are not in the file but in the wave data that belongs to it in the sound archive.
struct BankFile
{
    std::vector<Instrument> instruments;

    // A bank of version 1.1 describes its waves itself, and the wave data holds their samples one after the other. A bank of
    // version 1.2 describes none: its wave data is a wave archive (RWAR), and the notes number its waves.
    bool describesItsWaves = true;
    std::vector<WaveInfo> waves;

    // The note that the instrument plays for the key and velocity, both 0 to 127.
    // Gives nothing when the instrument does not exist or has no note for them.
    std::optional<NoteParameters> FindNote(std::size_t instrumentNumber, int key, int velocity) const;
};

BankFile ReadBankFile(Formats::ByteView bytes);

// Fails when a note plays a wave that is not among the waveCount waves.
void CheckThatTheWavesExist(const BankFile& bankFile, std::size_t waveCount);

}
