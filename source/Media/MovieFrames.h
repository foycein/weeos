#pragma once

#include "Media/Movie.h"

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

namespace Media
{

// Decodes one frame of a Motion-JPEG video track. A frame of two fields holds two pictures of half the height; the first one
// gives the even lines (0, 2, 4 ...) and the second one the odd lines. Throws a FormatError for the kind Unsupported and for
// a frame that cannot be decoded.
Formats::RgbaImage DecodeMovieFrame(const MovieVideoTrack& video, Formats::ByteView frameBytes);

// A field of Motion-JPEG B written again as a plain JPEG file: the markers in front of its segments, byte stuffing in its data.
Formats::Bytes MotionJpegBFieldToJpeg(Formats::ByteView field);

}
