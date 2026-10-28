#pragma once

#include "Formats/Bytes.h"
#include "Formats/Signature.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

enum class PublicKeyType : std::uint32_t
{
    Rsa4096 = 0,
    Rsa2048 = 1,
    EllipticCurve = 2,
};

struct Certificate
{
    Signature signature;
    std::string issuer;
    PublicKeyType keyType;
    std::string name;
    std::uint32_t keyId;

    // For an RSA key this is the modulus. For an elliptic curve key it is the point on the curve.
    Bytes publicKey;

    // Only RSA keys have an exponent. It is zero for an elliptic curve key.
    std::uint32_t publicExponent;
};

// The certificates a title is signed with, one after the other (the Wii uses three).
std::vector<Certificate> ReadCertificateChain(ByteView bytes);

}
