#pragma once

#include "Formats/Bytes.h"
#include "Formats/Sha1.h"

#include <cstddef>
#include <optional>

namespace Formats
{

// The Wii signs with elliptic curve cryptography on the curve sect233r1 (NIST B-233): the keys of consoles, the signatures of save data
// and the certificates of the manufacturing servers. A number on the curve is written in 30 bytes, the most significant byte first.
constexpr std::size_t EllipticCurveNumberSize = 30;

// A public key is a point on the curve: the two numbers x and y, one after the other.
constexpr std::size_t EllipticCurvePublicKeySize = 2 * EllipticCurveNumberSize;

// A signature are the two numbers r and s, one after the other.
constexpr std::size_t EllipticCurveSignatureSize = 2 * EllipticCurveNumberSize;

// The public key that belongs to a private key. A private key is taken modulo the order of the curve, as the console does.
// Throws a FormatError when the private key has the wrong size or comes out as zero.
Bytes DerivePublicKey(ByteView privateKey);

// Signs a hash. The nonce can be any bytes, and should be 40 or more random bytes that are used only once: the signature gives away the
// private key to whoever learns the nonce, or sees two signatures made with the same one. The nonce is turned into a number between
// 1 and the order of the curve minus one. Gives back nothing in the very rare case that this nonce does not make a signature.
std::optional<Bytes> SignHash(ByteView privateKey, const Sha1Digest& hash, ByteView nonce);

// True when the signature is the signature of the hash that belongs to the public key.
// A public key that is not a point on the curve, and a signature of the wrong size or with numbers out of range, are not valid.
bool VerifyHashSignature(ByteView publicKey, const Sha1Digest& hash, ByteView signature);

}
