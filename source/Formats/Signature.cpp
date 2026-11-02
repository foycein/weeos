#include "Formats/Signature.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t Rsa4096SignatureSize = 512;
constexpr std::size_t Rsa2048SignatureSize = 256;
constexpr std::size_t EllipticCurveSignatureSize = 60;
constexpr std::size_t RsaPaddingSize = 60;

// The padding after an elliptic curve signature is 64 bytes: the device certificate of a real console
// is 384 bytes only with it, and its issuer name starts at offset 0x80.
constexpr std::size_t EllipticCurvePaddingSize = 64;

struct SignatureLayout
{
    std::size_t signatureSize;
    std::size_t paddingSize;
};

SignatureLayout LayoutOf(SignatureType type, const ByteReader& reader)
{
    switch (type)
    {
    case SignatureType::Rsa4096:
        return {Rsa4096SignatureSize, RsaPaddingSize};

    case SignatureType::Rsa2048:
        return {Rsa2048SignatureSize, RsaPaddingSize};

    case SignatureType::EllipticCurve:
        return {EllipticCurveSignatureSize, EllipticCurvePaddingSize};
    }

    reader.Fail(std::format("the signature type {:#x} is not known", static_cast<std::uint32_t>(type)));
}

}

Signature ReadSignature(ByteReader& reader)
{
    Signature signature;
    signature.type = static_cast<SignatureType>(reader.ReadUint32());

    SignatureLayout layout = LayoutOf(signature.type, reader);
    ByteView signatureBytes = reader.ReadBytes(layout.signatureSize);
    signature.bytes.assign(signatureBytes.begin(), signatureBytes.end());

    reader.Skip(layout.paddingSize);
    return signature;
}

}
