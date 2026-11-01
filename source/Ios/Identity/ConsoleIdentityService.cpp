#include "Ios/Identity/ConsoleIdentityService.h"

#include "Formats/Certificate.h"
#include "Formats/DeviceCertificate.h"
#include "Formats/EllipticCurve.h"
#include "Formats/FileBytes.h"
#include "Formats/FormatError.h"
#include "Formats/SerialEeprom.h"
#include "Formats/Sha1.h"
#include "Formats/SignatureVerification.h"
#include "Ios/Identity/IdentityFiles.h"

#include <algorithm>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace Ios
{

namespace
{

constexpr std::size_t DeviceCertificateSize = 0x180;

// The signature needs a number that nobody can guess, from 40 bytes up. See SignHash.
constexpr std::size_t NonceSize = 48;

// A certificate names its issuer by the chain above it: the issuer of the issuer, a dash, the name of the issuer ("Root-WeeOS").
constexpr char IssuerSeparator = '-';

Result<Formats::Bytes> ReadIdentityFile(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path))
        return IosError::NotFound;

    try
    {
        return Formats::ReadFileBytes(path);
    }
    catch (const std::exception&)
    {
        return IosError::StorageFailure;
    }
}

Formats::Bytes MakeRandomBytes(std::size_t count)
{
    std::random_device randomDevice;
    std::uniform_int_distribution<int> anyByte(0, Formats::ByteMask);

    Formats::Bytes bytes(count);
    for (Formats::Byte& byte : bytes)
        byte = static_cast<Formats::Byte>(anyByte(randomDevice));

    return bytes;
}

// The last block of the data, the one that the next call continues with in cipher block chaining mode.
Formats::AesBlock LastBlockOrDefault(Formats::ByteView bytes, const Formats::AesBlock& defaultBlock)
{
    if (bytes.empty())
        return defaultBlock;

    Formats::AesBlock block;
    std::copy(bytes.end() - Formats::AesBlockSize, bytes.end(), block.begin());
    return block;
}

bool IsWholeBlocks(Formats::ByteView data)
{
    return data.size() % Formats::AesBlockSize == 0;
}

bool IsZero(Formats::Byte byte)
{
    return byte == 0;
}

std::string NameAsIssuer(const Formats::Certificate& certificate)
{
    return certificate.issuer + IssuerSeparator + certificate.name;
}

// A console that WeeOS made has the certificate of its own authority. A real console has none.
Result<std::optional<Formats::Certificate>> ReadAuthorityCertificate(const std::filesystem::path& identityFolder)
{
    std::filesystem::path path = identityFolder / AuthorityCertificateFileName;
    if (!std::filesystem::exists(path))
        return std::optional<Formats::Certificate>();

    Result<Formats::Bytes> bytes = ReadIdentityFile(path);
    if (bytes.Failed())
        return bytes.Error();

    std::vector<Formats::Certificate> certificates = Formats::ReadCertificateChain(bytes.Value());
    bool isOneCertificate = certificates.size() == 1;
    if (!isOneCertificate || certificates.front().keyType != Formats::PublicKeyType::EllipticCurve)
        return IosError::ShortRead;

    return std::optional<Formats::Certificate>(certificates.front());
}

}

ConsoleIdentityService::ConsoleIdentityService(Formats::ConsoleKeys consoleKeys, Formats::Bytes certificate,
    std::uint8_t consoleBootTwoVersion, bool consoleHasKoreanKey, std::optional<Formats::Certificate> ownAuthority)
    : keys(consoleKeys)
    , deviceCertificate(std::move(certificate))
    , bootTwoVersion(consoleBootTwoVersion)
    , hasKoreanKey(consoleHasKoreanKey)
    , authority(std::move(ownAuthority))
{
}

Result<ConsoleIdentityService> ConsoleIdentityService::Open(const std::filesystem::path& identityFolder)
{
    Result<Formats::Bytes> keysBytes = ReadIdentityFile(identityFolder / KeysFileName);
    if (keysBytes.Failed())
        return keysBytes.Error();

    Result<Formats::Bytes> certificateBytes = ReadIdentityFile(identityFolder / DeviceCertificateFileName);
    if (certificateBytes.Failed())
        return certificateBytes.Error();

    Result<Formats::Bytes> serialEepromBytes = ReadIdentityFile(identityFolder / SerialEepromFileName);
    if (serialEepromBytes.Failed())
        return serialEepromBytes.Error();

    try
    {
        Formats::ConsoleKeys keys = Formats::ReadConsoleKeys(keysBytes.Value());
        Formats::DeviceCertificate certificate = Formats::ReadDeviceCertificate(certificateBytes.Value());
        Formats::SerialEeprom serialEeprom = Formats::ReadSerialEeprom(serialEepromBytes.Value());

        bool certificateBelongsToTheKeys = certificate.consoleId == keys.consoleId;
        bool hasCertificateSize = certificateBytes.Value().size() == DeviceCertificateSize;
        if (!certificateBelongsToTheKeys || !hasCertificateSize)
            return IosError::ShortRead;

        Result<std::optional<Formats::Certificate>> authority = ReadAuthorityCertificate(identityFolder);
        if (authority.Failed())
            return authority.Error();

        bool hasKoreanKey = !std::all_of(serialEeprom.koreanKey.begin(), serialEeprom.koreanKey.end(), IsZero);
        return ConsoleIdentityService(keys, certificateBytes.Value(), serialEeprom.bootTwoVersion, hasKoreanKey, authority.Value());
    }
    catch (const Formats::FormatError&)
    {
        return IosError::ShortRead;
    }
}

std::uint32_t ConsoleIdentityService::GetDeviceId() const
{
    return keys.consoleId;
}

const Formats::Bytes& ConsoleIdentityService::GetDeviceCertificate() const
{
    return deviceCertificate;
}

SignedData ConsoleIdentityService::Sign(Formats::ByteView data) const
{
    Formats::Sha1Digest hash = Formats::ComputeSha1(data);

    while (true)
    {
        Formats::Bytes nonce = MakeRandomBytes(NonceSize);
        std::optional<Formats::Bytes> signature = Formats::SignHash(keys.ellipticCurvePrivateKey, hash, nonce);
        if (signature)
            return SignedData{*signature, deviceCertificate};
    }
}

Result<void> ConsoleIdentityService::VerifySignature(Formats::ByteView data, Formats::ByteView signature,
    Formats::ByteView certificateChain) const
{
    std::vector<Formats::Certificate> certificates;
    try
    {
        certificates = Formats::ReadCertificateChain(certificateChain);
    }
    catch (const Formats::FormatError&)
    {
        return IosError::TitleInvalidArgument;
    }

    if (certificates.empty())
        return IosError::TitleInvalidArgument;

    const Formats::Certificate& signer = certificates.back();
    if (signer.keyType != Formats::PublicKeyType::EllipticCurve)
        return IosError::PublicKeyTypeInvalid;

    Formats::SignatureCheck chainCheck = CheckCertificate(signer, certificates);
    if (chainCheck == Formats::SignatureCheck::SignerNotFound)
        return IosError::IssuerNotFound;

    if (chainCheck == Formats::SignatureCheck::Invalid)
        return IosError::SignatureInvalid;

    if (!Formats::VerifyHashSignature(signer.publicKey, Formats::ComputeSha1(data), signature))
        return IosError::SignatureInvalid;

    return Result<void>();
}

// A certificate that the own authority issued is checked with the key of the authority, and the chain ends there.
Formats::SignatureCheck ConsoleIdentityService::CheckCertificate(const Formats::Certificate& certificate,
    std::span<const Formats::Certificate> certificates) const
{
    bool isIssuedByOwnAuthority = authority && certificate.issuer == NameAsIssuer(*authority);
    if (!isIssuedByOwnAuthority)
        return Formats::VerifyCertificate(certificate, certificates);

    Formats::Sha1Digest hash = Formats::ComputeSha1(certificate.signedBytes);
    if (!Formats::VerifyHashSignature(authority->publicKey, hash, certificate.signature.bytes))
        return Formats::SignatureCheck::Invalid;

    return Formats::SignatureCheck::Valid;
}

Result<CipherResult> ConsoleIdentityService::Encrypt(std::uint32_t keyNumber, const Formats::AesBlock& initializationVector,
    Formats::ByteView data) const
{
    if (keyNumber != SdCardKeyNumber || !IsWholeBlocks(data))
        return IosError::TitleInvalidArgument;

    CipherResult result;
    result.data = Formats::EncryptAes128Cbc(data, keys.sdKey, initializationVector);
    result.nextInitializationVector = LastBlockOrDefault(result.data, initializationVector);
    return result;
}

Result<CipherResult> ConsoleIdentityService::Decrypt(std::uint32_t keyNumber, const Formats::AesBlock& initializationVector,
    Formats::ByteView data) const
{
    if (keyNumber != SdCardKeyNumber || !IsWholeBlocks(data))
        return IosError::TitleInvalidArgument;

    CipherResult result;
    result.data = Formats::DecryptAes128Cbc(data, keys.sdKey, initializationVector);
    result.nextInitializationVector = LastBlockOrDefault(data, initializationVector);
    return result;
}

std::uint32_t ConsoleIdentityService::GetBootTwoVersion() const
{
    return bootTwoVersion;
}

bool ConsoleIdentityService::IsKoreanConsole() const
{
    return hasKoreanKey;
}

}
