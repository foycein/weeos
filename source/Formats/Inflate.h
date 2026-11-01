#pragma once

#include "Formats/Bytes.h"

namespace Formats
{

// Unpacks data that was compressed with deflate and wrapped as a zlib stream, the way the pictures of PNG files are kept.
// Throws a FormatError when the data is damaged. The check number at the end of the stream is not compared.
Bytes InflateZlib(ByteView zlibBytes);

}
