#include "Ios/Sockets/CertificateCheck.h"

#include <windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <memory>

namespace Ios
{

namespace
{

constexpr DWORD CertificateEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

struct CertificateRelease
{
    void operator()(const CERT_CONTEXT* certificate) const
    {
        CertFreeCertificateContext(certificate);
    }
};

using WindowsCertificate = std::unique_ptr<const CERT_CONTEXT, CertificateRelease>;

WindowsCertificate ReadCertificate(Formats::ByteView certificate)
{
    return WindowsCertificate(CertCreateCertificateContext(CertificateEncoding, certificate.data(), static_cast<DWORD>(certificate.size())));
}

bool IsSameCertificate(Formats::ByteView first, Formats::ByteView second)
{
    return first.size() == second.size() && std::equal(first.begin(), first.end(), second.begin());
}

const Formats::Bytes* FindIssuer(std::span<const Formats::Bytes> certificates, const Formats::Bytes& certificate)
{
    for (const Formats::Bytes& candidate : certificates)
    {
        if (&candidate == &certificate)
            continue;

        if (IsSignedBy(certificate, candidate))
            return &candidate;
    }
    return nullptr;
}

}

std::string CommonNameOf(Formats::ByteView certificate)
{
    WindowsCertificate windowsCertificate = ReadCertificate(certificate);
    if (!windowsCertificate)
        return "";

    void* commonNameAttribute = const_cast<char*>(szOID_COMMON_NAME);
    DWORD size = CertGetNameStringA(windowsCertificate.get(), CERT_NAME_ATTR_TYPE, 0, commonNameAttribute, nullptr, 0);
    std::string name(size, '\0');
    CertGetNameStringA(windowsCertificate.get(), CERT_NAME_ATTR_TYPE, 0, commonNameAttribute, name.data(), size);

    // Windows counts the zero at the end of the text.
    name.resize(size - 1);
    return name;
}

bool IsSignedBy(Formats::ByteView certificate, Formats::ByteView issuer)
{
    WindowsCertificate signedCertificate = ReadCertificate(certificate);
    WindowsCertificate issuerCertificate = ReadCertificate(issuer);
    if (!signedCertificate || !issuerCertificate)
        return false;

    BOOL isVerified = CryptVerifyCertificateSignatureEx(0, X509_ASN_ENCODING, CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
        const_cast<CERT_CONTEXT*>(signedCertificate.get()), CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
        const_cast<CERT_CONTEXT*>(issuerCertificate.get()), 0, nullptr);
    return isVerified == TRUE;
}

bool IsInDate(Formats::ByteView certificate)
{
    WindowsCertificate windowsCertificate = ReadCertificate(certificate);
    if (!windowsCertificate)
        return false;

    return CertVerifyTimeValidity(nullptr, windowsCertificate->pCertInfo) == 0;
}

bool IsTrustedByRoot(std::span<const Formats::Bytes> serverCertificates, Formats::ByteView root)
{
    for (const Formats::Bytes& certificate : serverCertificates)
    {
        if (IsSameCertificate(certificate, root) || IsSignedBy(certificate, root))
            return true;
    }
    return false;
}

bool IsChainUnbroken(std::span<const Formats::Bytes> serverCertificates, Formats::ByteView root)
{
    if (serverCertificates.empty())
        return false;

    const Formats::Bytes* current = &serverCertificates.front();

    // A chain cannot be longer than the list; a longer walk goes round in a circle.
    for (std::size_t step = 0; step <= serverCertificates.size(); ++step)
    {
        if (IsSignedBy(*current, *current))
            return true;

        if (!root.empty() && IsSignedBy(*current, root))
            return true;

        current = FindIssuer(serverCertificates, *current);
        if (current == nullptr)
            return false;
    }
    return false;
}

}
