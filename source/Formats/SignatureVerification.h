#pragma once

#include "Formats/Bytes.h"
#include "Formats/Certificate.h"

#include <span>

namespace Formats
{

enum class SignatureCheck
{
    Valid,

    // The file or certificate names a signer, and there is no certificate of that signer in the list.
    // Also the answer for a chain of certificates that does not end at the root.
    SignerNotFound,

    // The signature does not fit the data: the data was changed, or it was signed by somebody else.
    Invalid,
};

// Checks the signature of a ticket, a title metadata or a certificate. Such a file starts with its signature, and the signature covers
// all that comes after it. The name of the signer is the first thing behind the signature, for example "Root-CA00000001-XS00000003":
// the certificate XS00000003, which was signed by CA00000001, which was signed by the root.
//
// The certificate of the signer must be in the list, and so must the certificate of every signer above it, except the root,
// whose key is built in. All of them are checked: the answer is Valid only when the file is signed by the signer,
// the signer is signed by its signer, and so on up to the root.
SignatureCheck VerifySignedFile(ByteView signedFile, std::span<const Certificate> certificates);

// Checks the signature of a certificate and then the certificates above it, up to the root, in the same way.
SignatureCheck VerifyCertificate(const Certificate& certificate, std::span<const Certificate> certificates);

}
