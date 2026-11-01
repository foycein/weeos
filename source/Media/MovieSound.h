#pragma once

#include "Media/Movie.h"
#include "Media/PcmSound.h"

#include "Formats/Bytes.h"

namespace Media
{

// Decodes the whole sound track of a movie into 16 bit samples. Throws a FormatError for the kinds Unsupported and Aac
// (AAC has its own decoder) and for pieces that lie outside the file.
PcmSound DecodeMovieSound(const MovieSoundTrack& sound, Formats::ByteView fileBytes);

// One sample of G.711 (ITU-T): mu-law (North America and Japan) and A-law (Europe), 8 bits for 14 or 13 bits of sound.
std::int16_t DecodeMuLaw(Formats::Byte value);
std::int16_t DecodeALaw(Formats::Byte value);

// One block of IMA ADPCM as WAV and AVI files store it: for each channel the first sample and the step index, then the
// differences, 4 bits each, in groups of 4 bytes (8 samples) per channel in turn.
void DecodeImaAdpcmBlock(Formats::ByteView block, int channelCount, int samplesPerBlock, std::vector<std::int16_t>& samples);

}
