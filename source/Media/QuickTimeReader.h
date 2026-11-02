#pragma once

#include "Media/Movie.h"

#include "Formats/Bytes.h"

namespace Media
{

// Reads the tracks of a QuickTime movie (MOV) or an MP4 file (M4A): the first video track and the first sound track, with the
// place of every frame and every piece of sound from the sample tables (stsd, stts, stsc, stsz, stco or co64).
//
// The kinds are the ones the Photo Channel plays: video "jpeg", "mjpa"
// (Motion-JPEG A) and "mjpb" (Motion-JPEG B) with one or two fields; sound "raw ", "twos", "sowt", "ulaw", "alaw", and "mp4a"
// (AAC, for the music of the slide show). Any other codec gives the kind Unsupported with its name.
// Throws a FormatError when the file has no movie box.
Movie ReadQuickTimeMovie(Formats::ByteView fileBytes);

}
