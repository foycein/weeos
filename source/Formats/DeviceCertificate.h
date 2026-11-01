#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

// The certificate of the console (the file device.cert). Its name is "NG" and the console id, for example "NG0a447b97".
// It holds the public key that belongs to the private key of the console and is signed at the factory.
struct DeviceCertificate
{
    // The signers from the root down to the one that signed this certificate, for example {"Root", "CA00000001", "MS00000002"}.
    std::vector<std::string> issuerChain;

    std::uint32_t consoleId;
    std::uint32_t keyId;

    // A point on an elliptic curve (60 bytes), and the signature of the signer (60 bytes).
    Bytes publicKey;
    Bytes signature;
};

// Throws a FormatError when the data is not exactly one certificate with an elliptic curve key and a name like "NG0a447b97".
DeviceCertificate ReadDeviceCertificate(ByteView bytes);

// The name of the certificate of a console: "NG" and the console id in eight small hexadecimal digits.
std::string DeviceCertificateName(std::uint32_t consoleId);

// The fields of a certificate with an elliptic curve key, signed with an elliptic curve key.
struct EllipticCurveCertificateFields
{
    std::string issuer;
    std::string name;
    std::uint32_t keyId = 0;

    // 60 bytes each.
    Bytes publicKey;
    Bytes signature;
};

// The 0x180 bytes of such a certificate, the layout of device.cert. The signature covers the bytes from the issuer to the end
// (Certificate::signedBytes), so a signer writes the certificate once with any signature, signs those bytes and writes it again.
// Throws a FormatError when a name does not fit its 64 bytes or a key or signature has the wrong size.
Bytes WriteEllipticCurveCertificate(const EllipticCurveCertificateFields& fields);

}
