#pragma once

#include "Formats/Bytes.h"
#include "Formats/Sha1.h"

namespace Formats
{

// HMAC with SHA-1 (RFC 2104): a hash that only whoever knows the key can make. The NAND file system protects every cluster with one.
// A key longer than 64 bytes is hashed first, as the standard says.
Sha1Digest ComputeHmacSha1(ByteView key, ByteView message);

}
