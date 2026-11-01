#pragma once

#include "Sound/WaveDecoder.h"

#include "Formats/Bytes.h"

namespace Sound
{

// Reads a stream file ("BNS ": the sound of a channel's banner behind its IMD5 header, the music of the Wii Speak Channel's performance
// check) and decodes all its channels: a header, the chunk INFO (codec, loop, channels, rate, loop start, sample count, and for each
// channel where its samples start and its DSP-ADPCM parameters) and the chunk DATA. Only DSP-ADPCM streams exist.
// Throws a FormatError when the file is not one.
DecodedWave ReadStreamFile(Formats::ByteView bytes);

}
