#pragma once

#include "Sound/WaveDecoder.h"

#include "Formats/Bytes.h"

namespace Sound
{

// The sound of a channel's banner, the file meta/sound.bin of its banner content (docs/research/Channels.md 1.2), which the channel
// start window plays while it shows the banner. A header of 32 bytes (IMD5) comes first, then a BNS stream: DSP-ADPCM samples in one
// or two channels, 32000 Hz in the channels of the console, sometimes with a loop.
// Throws a FormatError when the file is not such a sound.
DecodedWave ReadBannerSound(Formats::ByteView soundFile);

}
