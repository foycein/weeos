#include "Formats/Certificate.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t NameFieldSize = 64;
constexpr std::size_t Rsa4096ModulusSize = 512;
constexpr std::size_t Rsa2048ModulusSize = 256;
constexpr std::size_t RsaPaddingSize = 52;
constexpr std::size_t EllipticCurvePointSize = 60;
constexpr std::size_t EllipticCurvePaddingSize = 60;

PublicKeyType ReadPublicKeyType(ByteReader& reader)
{
    std::uint32_t number = reader.ReadUint32();

    if (number > static_cast<std::uint32_t>(PublicKeyType::EllipticCurve))
        reader.Fail(std::format("the public key type {:#x} is not known", number));

    return static_cast<PublicKeyType>(number);
}

void ReadRsaKey(ByteReader& reader, std::size_t modulusSize, Certificate& certificate)
{
    ByteView modulus = reader.ReadBytes(modulusSize);
    certificate.publicKey.assign(modulus.begin(), modulus.end());
    certificate.publicExponent = reader.ReadUint32();
    reader.Skip(RsaPaddingSize);
}

void ReadEllipticCurveKey(ByteReader& reader, Certificate& certificate)
{
    ByteView point = reader.ReadBytes(EllipticCurvePointSize);
    certificate.publicKey.assign(point.begin(), point.end());
    certificate.publicExponent = 0;
    reader.Skip(EllipticCurvePaddingSize);
}

void ReadPublicKey(ByteReader& reader, Certificate& certificate)
{
    switch (certificate.keyType)
    {
    case PublicKeyType::Rsa4096:
        ReadRsaKey(reader, Rsa4096ModulusSize, certificate);
        return;

    case PublicKeyType::Rsa2048:
        ReadRsaKey(reader, Rsa2048ModulusSize, certificate);
        return;

    case PublicKeyType::EllipticCurve:
        ReadEllipticCurveKey(reader, certificate);
        return;
    }
}

Certificate ReadCertificate(ByteReader& reader)
{
    Certificate certificate;
    certificate.signature = ReadSignature(reader);

    std::size_t signedStart = reader.Position();
    certificate.issuer = reader.ReadText(NameFieldSize);
    certificate.keyType = ReadPublicKeyType(reader);
    certificate.name = reader.ReadText(NameFieldSize);
    certificate.keyId = reader.ReadUint32();
    ReadPublicKey(reader, certificate);

    std::size_t signedEnd = reader.Position();
    reader.MoveTo(signedStart);
    ByteView signedBytes = reader.ReadBytes(signedEnd - signedStart);
    certificate.signedBytes.assign(signedBytes.begin(), signedBytes.end());
    return certificate;
}

}

std::vector<Certificate> ReadCertificateChain(ByteView bytes)
{
    ByteReader reader(bytes, "Certificate chain");

    std::vector<Certificate> certificates;
    while (!reader.IsAtEnd())
        certificates.push_back(ReadCertificate(reader));

    return certificates;
}

}
