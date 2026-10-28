#include "Formats/DeviceCertificate.h"

#include "Formats/ByteReader.h"
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
