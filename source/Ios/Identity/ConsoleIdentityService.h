#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/ConsoleKeys.h"
#include "Ios/Result.h"

#include <cstdint>
#include <filesystem>

namespace Ios
{

// What signing gives back: the signature, and the certificate of the console that the signature can be checked with.
struct SignedData
{
    Formats::Bytes signature;
    Formats::Bytes certificate;
};

// The data after it was encrypted or decrypted, and the initialization vector to continue with when the data goes on in another call.
struct CipherResult
{
    Formats::Bytes data;
    Formats::AesBlock nextInitializationVector;
};

// Who this console is: its number, its certificate, its private key and the keys it protects data with.
// It takes the place of the parts of ES that belong to the console, and of the crypto core of IOS that ES asks (docs/audit/Ios.md).
//
// WeeOS is the reference console of the dump: its identity is read from the files keys.txt, device.cert and seeprom.bin.
// The service never changes after it was opened, so calls from several threads are safe.
class ConsoleIdentityService
{
public:
    // The key number that the System Menu gives to Encrypt and Decrypt for the data it moves to the SD card.
    static constexpr std::uint32_t SdCardKeyNumber = 6;

    // Reads the identity from the three files in the folder.
    // NotFound when a file is missing, ShortRead when a file is not what it should be.
    static Result<ConsoleIdentityService> Open(const std::filesystem::path& identityFolder);

    // The number of the console (the NG id).
    std::uint32_t GetDeviceId() const;

    // The certificate of the console, 0x180 bytes: its name, its public key, and the signature of the factory.
    const Formats::Bytes& GetDeviceCertificate() const;

    // Signs the SHA-1 hash of the data with the private key of the console. The certificate in the answer checks the signature.
    // (The console makes a new key and a certificate of its own for every signature; this one signs with its device key, docs/audit/OpenPoints/Ios.md 6.)
    SignedData Sign(Formats::ByteView data) const;

    // Checks a signature that the last certificate of the chain has made over the SHA-1 hash of the data. The certificates are given
    // one after the other as they are stored in files. The chain must lead from that certificate up to the root.
    //  - TitleInvalidArgument: the certificates cannot be read, or there are none.
    //  - PublicKeyTypeInvalid: the last certificate has no elliptic curve key.
    //  - IssuerNotFound: a certificate of the chain is missing.
    //  - SignatureInvalid: a signature does not fit.
    Result<void> VerifySignature(Formats::ByteView data, Formats::ByteView signature, Formats::ByteView certificateChain) const;

    // Encrypts and decrypts data with a key of the console, in cipher block chaining mode, with the given initialization vector.
    // The only key is the one for the SD card (SdCardKeyNumber). TitleInvalidArgument for any other number
    // and for data that is not whole blocks of 16 bytes.
    Result<CipherResult> Encrypt(std::uint32_t keyNumber, const Formats::AesBlock& initializationVector, Formats::ByteView data) const;
    Result<CipherResult> Decrypt(std::uint32_t keyNumber, const Formats::AesBlock& initializationVector, Formats::ByteView data) const;

    // The version of boot2.
    std::uint32_t GetBootTwoVersion() const;

    // True when the console has the key that only Korean consoles have.
    bool IsKoreanConsole() const;

private:
    ConsoleIdentityService(Formats::ConsoleKeys consoleKeys, Formats::Bytes certificate, std::uint8_t consoleBootTwoVersion,
        bool consoleHasKoreanKey);

    Formats::ConsoleKeys keys;
    Formats::Bytes deviceCertificate;
    std::uint8_t bootTwoVersion;
    bool hasKoreanKey;
};

}
