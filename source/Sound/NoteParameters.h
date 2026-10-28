#pragma once

#include "Formats/ByteReader.h"

#include <cstdint>

namespace Sound
{

// How one recorded wave is played as a note. Banks use it for their instruments, wave sound files for their notes.
struct NoteParameters
{
    // The number of the wave in the wave list of the file. Negative when the note has no wave.
    std::int32_t waveIndex;

    // How the loudness of the note rises, falls and dies away. Each is a number from 0 to 127,
    // see Envelope.h for what they mean.
    std::uint8_t attack;
    std::uint8_t decay;
    std::uint8_t sustain;
    std::uint8_t release;

    // The key at which the wave sounds as it was recorded.
    std::uint8_t originalKey;

    // 0 to 127.
    std::uint8_t volume;

    // 0 is the left, 64 the middle and 127 the right.
    std::uint8_t pan;

    std::uint8_t surroundPan;

    // The pitch of the wave as a factor. 1.0 plays the wave at its own pitch.
    float tune;
};

// Reads the 48 bytes of a note. Vibrato, volume curves and random changes are not supported and make the reader fail.
NoteParameters ReadNoteParameters(Formats::ByteReader& reader);

}
