#pragma once

#include "Formats/Bytes.h"
#include "Formats/Sha1.h"

#include <cstdint>

namespace Formats
{

// True when the signature is the RSA signature of the hash with the public key (the modulus and the exponent).
// Nintendo signs tickets, title metadata and certificates like this: the hash is wrapped in the padding of PKCS #1 version 1.5
// (zero, one, bytes of 0xFF, zero, the name of the hash algorithm, the hash) and the whole block is raised to the secret exponent.
// The signature must be as long as the modulus. The block it gives must be exactly that padding and the hash.
bool VerifyRsaSha1Signature(ByteView modulus, std::uint32_t publicExponent, ByteView signature, const Sha1Digest& hash);

}
