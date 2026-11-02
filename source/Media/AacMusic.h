#pragma once

#include "Media/PcmSound.h"

#include "Formats/Bytes.h"

namespace Media
{

// Decodes the music of an AAC file (.m4a) with Windows' own AAC decoder (Media Foundation), only for music files.
// Gives 16 bit samples at the file's own rate and channels. Throws a FormatError when Windows cannot read the file.
PcmSound DecodeAacMusic(Formats::ByteView m4aFile);

}
