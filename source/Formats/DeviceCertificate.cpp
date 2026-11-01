#include "Formats/DeviceCertificate.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/Certificate.h"
#include "Formats/FormatError.h"
#include "Formats/HexText.h"

#include <format>
#include <string_view>

namespace Formats
{

namespace
{

constexpr std::string_view NamePrefix = "NG";
constexpr char IssuerSeparator = '-';

std::vector<std::string> SplitIssuer(const std::string& issuer)
{
    std::vector<std::string> signers;
    std::size_t partStart = 0;
    while (true)
    {
        std::size_t separatorPosition = issuer.find(IssuerSeparator, partStart);
        signers.push_back(issuer.substr(partStart, separatorPosition - partStart));
        if (separatorPosition == std::string::npos)
            return signers;

        partStart = separatorPosition + 1;
    }
}

std::uint32_t ReadConsoleIdFromName(const std::string& name)
{
    if (!name.starts_with(NamePrefix))
        throw FormatError(std::format("Device certificate: the name \"{}\" does not start with \"{}\"", name, NamePrefix));

    Bytes idBytes = ParseHexText(std::string_view(name).substr(NamePrefix.size()));
    ByteReader reader(idBytes, "Device certificate name");
    std::uint32_t consoleId = reader.ReadUint32();
    if (!reader.IsAtEnd())
        throw FormatError(std::format("Device certificate: the name \"{}\" has more than 8 hexadecimal digits", name));

    return consoleId;
}

constexpr std::size_t NameFieldSize = 64;
constexpr std::size_t EllipticCurveSignaturePaddingSize = 64;
constexpr std::size_t EllipticCurveKeySize = 60;
constexpr std::size_t EllipticCurveKeyPaddingSize = 60;

void WriteNameField(ByteWriter& writer, const std::string& name)
{
    if (name.size() >= NameFieldSize)
        throw FormatError(std::format("Certificate: the name \"{}\" does not fit in {} bytes", name, NameFieldSize));

    writer.WriteBytes(ByteView(reinterpret_cast<const Byte*>(name.data()), name.size()));
    writer.WriteZeros(NameFieldSize - name.size());
}

void RequireSize(const Bytes& bytes, std::size_t size, std::string_view what)
{
    if (bytes.size() != size)
        throw FormatError(std::format("Certificate: the {} has {} bytes instead of {}", what, bytes.size(), size));
}

}

std::string DeviceCertificateName(std::uint32_t consoleId)
{
    return std::format("{}{:08x}", NamePrefix, consoleId);
}

Bytes WriteEllipticCurveCertificate(const EllipticCurveCertificateFields& fields)
{
    RequireSize(fields.signature, EllipticCurveKeySize, "signature");
    RequireSize(fields.publicKey, EllipticCurveKeySize, "public key");

    ByteWriter writer;
    writer.WriteUint32(static_cast<std::uint32_t>(SignatureType::EllipticCurve));
    writer.WriteBytes(fields.signature);
    writer.WriteZeros(EllipticCurveSignaturePaddingSize);
    WriteNameField(writer, fields.issuer);
    writer.WriteUint32(static_cast<std::uint32_t>(PublicKeyType::EllipticCurve));
    WriteNameField(writer, fields.name);
    writer.WriteUint32(fields.keyId);
    writer.WriteBytes(fields.publicKey);
    writer.WriteZeros(EllipticCurveKeyPaddingSize);
    return writer.GetBytes();
}

DeviceCertificate ReadDeviceCertificate(ByteView bytes)
{
    std::vector<Certificate> chain = ReadCertificateChain(bytes);
    if (chain.size() != 1)
        throw FormatError(std::format("Device certificate: the data holds {} certificates instead of one", chain.size()));

    const Certificate& certificate = chain.front();
    if (certificate.keyType != PublicKeyType::EllipticCurve)
        throw FormatError("Device certificate: the key is not an elliptic curve key");

    DeviceCertificate device;
    device.issuerChain = SplitIssuer(certificate.issuer);
    device.consoleId = ReadConsoleIdFromName(certificate.name);
    device.keyId = certificate.keyId;
    device.publicKey = certificate.publicKey;
    device.signature = certificate.signature.bytes;
    return device;
}

}
