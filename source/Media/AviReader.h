#pragma once

#include "Media/Movie.h"

#include "Formats/Bytes.h"

namespace Media
{

// Reads the tracks of an AVI file (RIFF "AVI ", and the "AVIX" parts of an OpenDML file larger than 1 GB): the first video stream
// and the first sound stream, with the place of every frame and every chunk of sound in the "movi" lists.
//
// The kinds are the ones the Photo Channel plays (docs/audit/OpenPoints/Channels.md CHN-07): video "MJPG"; sound PCM of 8 bits
// (unsigned) or 16 bits, mu-law, and IMA ADPCM. Any other codec gives the kind Unsupported with its name.
// Throws a FormatError when the file is no AVI file.
Movie ReadAviMovie(Formats::ByteView fileBytes);

}
