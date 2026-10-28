#pragma once

#include "Sound/NoteParameters.h"
#include "Sound/WaveInfo.h"

#include "Formats/Bytes.h"

#include <cstdint>
#include <vector>

namespace Sound
{

// One sound of a wave sound file: a single wave that is played once from its start, with the volume and pitch of one note.
struct WaveSound
{
    // The pitch of the whole sound as a factor.
    float pitch;

    // 0 is the left, 64 the middle and 127 the right.
    std::uint8_t pan;
    std::uint8_t surroundPan;

    // How much of the sound goes to the reverb of the effect bus A, 0 to 127. The sends to the buses B and C are not kept,
    // because the menu gives those buses no effect.
    std::uint8_t effectSendA;

    // How much of the sound goes to the speakers, 0 to 127.
    std::uint8_t mainSend;

    NoteParameters note;
};

// A wave sound file (RWSD) holds the descriptions of some sounds and of the waves they play.
// The samples of the waves are not in the file but in the wave data that belongs to it in the sound archive.
struct WaveSoundFile
{
    std::vector<WaveSound> sounds;
    std::vector<WaveInfo> waves;
};

// The menu's wave sounds start their one note at the beginning and let it play to the end of the wave.
// Sounds with more notes or other timing are refused.
WaveSoundFile ReadWaveSoundFile(Formats::ByteView bytes);

}
