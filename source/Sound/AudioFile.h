#pragma once

#include "Sound/WaveDecoder.h"

#include "Formats/Bytes.h"

namespace Sound
{

// Reads a sound file that the wave player of the menu plays (0x8140E7B8 looks at the first four bytes): a WAV file ("RIFF", little
// endian samples) or an AIFF file ("FORM", big endian samples), with one or two channels of 8 or 16 bit samples. These are the sounds
// that can be attached to a message of the Message Board.
// Throws a FormatError when the file is none of these.
DecodedWave ReadAudioFile(Formats::ByteView file);

}
