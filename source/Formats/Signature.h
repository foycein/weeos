#pragma once

#include "Formats/ByteReader.h"
#include "Formats/Bytes.h"

#include <cstdint>

namespace Formats
{

enum class SignatureType : std::uint32_t
{
    Rsa4096 = 0x00010000,
    Rsa2048 = 0x00010001,
    EllipticCurve = 0x00010002,
};

// The start of every signed Wii file: the kind of signature, the signature itself and padding.
struct Signature
{
    SignatureType type;
    Bytes bytes;
};

Signature ReadSignature(ByteReader& reader);

}
