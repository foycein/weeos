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

}
