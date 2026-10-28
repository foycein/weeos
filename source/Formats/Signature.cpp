#include "Formats/Signature.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t Rsa4096SignatureSize = 512;
constexpr std::size_t Rsa2048SignatureSize = 256;
constexpr std::size_t EllipticCurveSignatureSize = 60;
constexpr std::size_t PaddingSize = 60;

std::size_t SignatureSizeOf(SignatureType type, const ByteReader& reader)
{
    switch (type)
    {
    case SignatureType::Rsa4096:
        return Rsa4096SignatureSize;

    case SignatureType::Rsa2048:
        return Rsa2048SignatureSize;

    case SignatureType::EllipticCurve:
        return EllipticCurveSignatureSize;
    }

    reader.Fail(std::format("the signature type {:#x} is not known", static_cast<std::uint32_t>(type)));
}

}

Signature ReadSignature(ByteReader& reader)
{
    Signature signature;
    signature.type = static_cast<SignatureType>(reader.ReadUint32());

    std::size_t signatureSize = SignatureSizeOf(signature.type, reader);
    ByteView signatureBytes = reader.ReadBytes(signatureSize);
    signature.bytes.assign(signatureBytes.begin(), signatureBytes.end());

    reader.Skip(PaddingSize);
    return signature;
}

}
