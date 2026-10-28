#include "Formats/SignatureVerification.h"

#include "Formats/ByteReader.h"
#include "Formats/EllipticCurve.h"
#include "Formats/FormatError.h"
#include "Formats/RsaSignature.h"
#include "Formats/Sha1.h"
#include "Formats/Signature.h"
#include "Formats/WiiRootKey.h"

#include <optional>
#include <string>
#include <string_view>

namespace Formats
{

namespace
{

constexpr std::size_t NameFieldSize = 64;
constexpr std::string_view RootName = "Root";
constexpr char NameSeparator = '-';

// A chain of certificates is a few steps long. A longer one has a loop in it.
constexpr int MaximumChainLength = 8;

Certificate MakeRootCertificate()
{
    Certificate root;
    root.keyType = PublicKeyType::Rsa4096;
    root.name = RootName;
    root.publicKey.assign(WiiRootModulus.begin(), WiiRootModulus.end());
    root.publicExponent = WiiRootPublicExponent;
    return root;
}

// The certificate of the signer that an issuer field names. The field lists the signers from the root down, like a path.
const Certificate* FindSigner(const std::string& issuer, std::span<const Certificate> certificates, const Certificate& root)
{
    if (issuer == RootName)
        return &root;

    std::size_t lastSeparator = issuer.rfind(NameSeparator);
    if (lastSeparator == std::string::npos)
        return nullptr;

    std::string signerName = issuer.substr(lastSeparator + 1);
    std::string signerIssuer = issuer.substr(0, lastSeparator);

    for (const Certificate& certificate : certificates)
    {
        if (certificate.name == signerName && certificate.issuer == signerIssuer)
            return &certificate;
    }
    return nullptr;
}

bool IsSignedBy(const Signature& signature, ByteView signedBytes, const Certificate& signer)
{
    Sha1Digest hash = ComputeSha1(signedBytes);

    if (signer.keyType == PublicKeyType::EllipticCurve)
        return VerifyHashSignature(signer.publicKey, hash, signature.bytes);

    return VerifyRsaSha1Signature(signer.publicKey, signer.publicExponent, signature.bytes, hash);
}

SignatureCheck CheckSignature(const Signature& signature, ByteView signedBytes, const Certificate& signer)
{
    return IsSignedBy(signature, signedBytes, signer) ? SignatureCheck::Valid : SignatureCheck::Invalid;
}

SignatureCheck VerifyCertificateAgainstTheList(const Certificate& certificate, std::span<const Certificate> certificates,
    const Certificate& root, int remainingChainLength)
{
    if (remainingChainLength == 0)
        return SignatureCheck::SignerNotFound;

    const Certificate* signer = FindSigner(certificate.issuer, certificates, root);
    if (signer == nullptr)
        return SignatureCheck::SignerNotFound;

    SignatureCheck check = CheckSignature(certificate.signature, certificate.signedBytes, *signer);
    if (check != SignatureCheck::Valid || signer == &root)
        return check;

    return VerifyCertificateAgainstTheList(*signer, certificates, root, remainingChainLength - 1);
}

// The signature of a file and what it covers.
struct SignedFileParts
{
    Signature signature;
    ByteView signedBytes;
    std::string issuer;
};

SignedFileParts SplitSignedFile(ByteView signedFile)
{
    ByteReader reader(signedFile, "Signed file");

    SignedFileParts parts;
    parts.signature = ReadSignature(reader);
    parts.signedBytes = reader.ReadBytes(reader.RemainingSize());

    ByteReader signedReader(parts.signedBytes, "Signed file");
    parts.issuer = signedReader.ReadText(NameFieldSize);
    return parts;
}

}

SignatureCheck VerifySignedFile(ByteView signedFile, std::span<const Certificate> certificates)
{
    SignedFileParts parts;
    try
    {
        parts = SplitSignedFile(signedFile);
    }
    catch (const FormatError&)
    {
        return SignatureCheck::Invalid;
    }

    Certificate root = MakeRootCertificate();
    const Certificate* signer = FindSigner(parts.issuer, certificates, root);
    if (signer == nullptr)
        return SignatureCheck::SignerNotFound;

    SignatureCheck check = CheckSignature(parts.signature, parts.signedBytes, *signer);
    if (check != SignatureCheck::Valid || signer == &root)
        return check;

    return VerifyCertificateAgainstTheList(*signer, certificates, root, MaximumChainLength);
}

SignatureCheck VerifyCertificate(const Certificate& certificate, std::span<const Certificate> certificates)
{
    Certificate root = MakeRootCertificate();
    return VerifyCertificateAgainstTheList(certificate, certificates, root, MaximumChainLength);
}

}
