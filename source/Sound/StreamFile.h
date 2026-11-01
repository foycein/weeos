#pragma once

#include "Sound/WaveDecoder.h"

#include "Formats/Bytes.h"

namespace Sound
{

// Reads a stream file of the channels ("BNS ", the music of a channel's banner and of the Wii Speak Channel's performance check) and
// decodes all its channels. The file has a header, an INFO section (encoding, loop, channels, rate, loop start, sample count and for
// each channel where its samples start and its DSP-ADPCM coefficients) and a DATA section with the samples of each channel one after
// the other. Only DSP-ADPCM streams exist. Throws a FormatError when the file is not one.
DecodedWave ReadStreamFile(Formats::ByteView bytes);

}
