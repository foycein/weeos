#pragma once

#include "Formats/Bytes.h"

#include <span>
#include <string>

namespace Ios
{

// The checks that the secure sockets make on the certificates a server sends. A certificate is in its binary form (DER), as the
// programs give their root certificate and as servers send them. Windows reads them.

// The common name of the certificate's subject. Empty when the bytes are not a certificate.
std::string CommonNameOf(Formats::ByteView certificate);

// The certificate carries a signature that the issuer's key verifies. False when either is not a certificate.
bool IsSignedBy(Formats::ByteView certificate, Formats::ByteView issuer);

// Now lies between the first and the last day of the certificate.
bool IsInDate(Formats::ByteView certificate);

// The root is one of the server's certificates, or it signed one of them.
bool IsTrustedByRoot(std::span<const Formats::Bytes> serverCertificates, Formats::ByteView root);

// From the server's own certificate, the first of the list, every certificate is signed by another one of the list, up to one that
// signed itself or that the root signed.
bool IsChainUnbroken(std::span<const Formats::Bytes> serverCertificates, Formats::ByteView root);

}
