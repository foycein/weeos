#include "Ios/Identity/IdentityFiles.h"

#include "Formats/BootMiiKeyBlock.h"
#include "Formats/Certificate.h"
#include "Formats/DeviceCertificate.h"
#include "Formats/EllipticCurve.h"
#include "Formats/FileBytes.h"
#include "Formats/FormatError.h"
#include "Formats/SdCardBackup.h"
#include "Formats/SerialEeprom.h"
#include "Formats/Sha1.h"
#include "Formats/WiiKeys.h"

#include <algorithm>
#include <format>
#include <random>

namespace Ios
{

namespace
{

constexpr std::string_view RootName = "Root";
constexpr std::string_view AuthorityName = "WeeOS";
constexpr char IssuerSeparator = '-';

// The SEEPROM lies in the key block of a BootMii backup at this offset (Formats::BootMiiKeyBlock).
constexpr std::size_t SerialEepromOffsetInKeyBlock = 0x200;

// A signature needs a number that nobody can guess, from 40 bytes up (Formats::SignHash).
constexpr std::size_t NonceSize = 48;

// Assumed: the version of boot2 that the reference console has. A console that WeeOS makes has no boot2; IOS only reports the number.
constexpr std::uint8_t NewConsoleBootTwoVersion = 5;

// The first byte of an address that a maker did not get from the IEEE: bit 1 says "set by the owner", bit 0 clear says "one device".
constexpr Formats::Byte LocallyAdministeredAddressBit = 0x02;
constexpr Formats::Byte GroupAddressBit = 0x01;

template <std::size_t Size>
std::array<Formats::Byte, Size> MakeRandomArray()
{
    std::random_device randomDevice;
    std::uniform_int_distribution<int> anyByte(0, Formats::ByteMask);

    std::array<Formats::Byte, Size> bytes;
    for (Formats::Byte& byte : bytes)
        byte = static_cast<Formats::Byte>(anyByte(randomDevice));

    return bytes;
}

std::uint32_t MakeRandomNumber()
{
    std::random_device randomDevice;
    return randomDevice();
}

MacAddress MakeNewMacAddress()
{
    MacAddress address = MakeRandomArray<Formats::MacAddressSize>();
    address[0] |= LocallyAdministeredAddressBit;
    address[0] &= static_cast<Formats::Byte>(~GroupAddressBit);
    return address;
}

// A private key of the curve. The rare key that comes out as zero modulo the order of the curve is made again.
std::array<Formats::Byte, Formats::EllipticCurvePrivateKeySize> MakePrivateKey()
{
    while (true)
    {
        std::array<Formats::Byte, Formats::EllipticCurvePrivateKeySize> privateKey = MakeRandomArray<Formats::EllipticCurvePrivateKeySize>();
        try
        {
            Formats::DerivePublicKey(privateKey);
            return privateKey;
        }
        catch (const Formats::FormatError&)
        {
        }
    }
}

Formats::Bytes SignBytes(Formats::ByteView privateKey, Formats::ByteView signedBytes)
{
    Formats::Sha1Digest hash = Formats::ComputeSha1(signedBytes);
    while (true)
    {
        std::array<Formats::Byte, NonceSize> nonce = MakeRandomArray<NonceSize>();
        std::optional<Formats::Bytes> signature = Formats::SignHash(privateKey, hash, nonce);
        if (signature)
            return *signature;
    }
}

// Writes the certificate with an empty signature first, because the signature covers the bytes behind it.
Formats::Bytes WriteSignedCertificate(Formats::EllipticCurveCertificateFields fields, Formats::ByteView signerPrivateKey)
{
    fields.signature.assign(Formats::EllipticCurveSignatureSize, 0);
    Formats::Bytes unsignedCertificate = Formats::WriteEllipticCurveCertificate(fields);
    Formats::Certificate certificate = Formats::ReadCertificateChain(unsignedCertificate).front();

    fields.signature = SignBytes(signerPrivateKey, certificate.signedBytes);
    return Formats::WriteEllipticCurveCertificate(fields);
}

// The keys that every Wii has, and the ones that only the NAND of a real console needs (they stay unused in a NAND folder).
Formats::ConsoleKeys MakeSharedKeys()
{
    Formats::ConsoleKeys keys{};
    keys.commonKey = Formats::WiiCommonKey;
    keys.sdKey = Formats::WiiSdKey;
    keys.sdInitializationVector = Formats::SdCardInitializationVector;
    keys.md5Blanker = Formats::HeaderDigestPlaceholder;
    return keys;
}

void WriteIdentityFiles(const std::filesystem::path& identityFolder, const Formats::ConsoleKeys& keys, Formats::ByteView deviceCertificate,
    Formats::ByteView serialEeprom)
{
    std::filesystem::create_directories(identityFolder);
    Formats::WriteFileBytes(identityFolder / KeysFileName, Formats::WriteConsoleKeys(keys));
    Formats::WriteFileBytes(identityFolder / DeviceCertificateFileName, deviceCertificate);
    Formats::WriteFileBytes(identityFolder / SerialEepromFileName, serialEeprom);
}

}

void WriteNewIdentity(const std::filesystem::path& identityFolder)
{
    std::array<Formats::Byte, Formats::EllipticCurvePrivateKeySize> authorityPrivateKey = MakePrivateKey();
    Formats::EllipticCurveCertificateFields authority;
    authority.issuer = RootName;
    authority.name = AuthorityName;
    authority.keyId = MakeRandomNumber();
    authority.publicKey = Formats::DerivePublicKey(authorityPrivateKey);
    Formats::Bytes authorityCertificate = WriteSignedCertificate(authority, authorityPrivateKey);

    Formats::ConsoleKeys keys = MakeSharedKeys();
    keys.consoleId = MakeRandomNumber();
    keys.ellipticCurvePrivateKey = MakePrivateKey();
    keys.nandHmacKey = MakeRandomArray<Formats::NandHmacKeySize>();
    keys.nandAesKey = MakeRandomArray<Formats::AesBlockSize>();
    keys.randomNumberKey = MakeRandomArray<Formats::AesBlockSize>();
    keys.ngKeyId = MakeRandomNumber();
    keys.macAddress = MakeNewMacAddress();

    Formats::EllipticCurveCertificateFields device;
    device.issuer = std::format("{}{}{}", RootName, IssuerSeparator, AuthorityName);
    device.name = Formats::DeviceCertificateName(keys.consoleId);
    device.keyId = keys.ngKeyId;
    device.publicKey = Formats::DerivePublicKey(keys.ellipticCurvePrivateKey);
    Formats::Bytes deviceCertificate = WriteSignedCertificate(device, authorityPrivateKey);

    Formats::DeviceCertificate readCertificate = Formats::ReadDeviceCertificate(deviceCertificate);
    std::copy(readCertificate.signature.begin(), readCertificate.signature.end(), keys.ngSignature.begin());

    Formats::SerialEeprom eeprom{};
    eeprom.ngKeyId = keys.ngKeyId;
    eeprom.ngSignature = keys.ngSignature;
    eeprom.bootTwoVersion = NewConsoleBootTwoVersion;

    WriteIdentityFiles(identityFolder, keys, deviceCertificate, Formats::WriteSerialEeprom(eeprom));
    Formats::WriteFileBytes(identityFolder / AuthorityCertificateFileName, authorityCertificate);
}

void WriteIdentityFromKeyBlock(Formats::ByteView keyBlock, const std::optional<MacAddress>& macAddress,
    const std::filesystem::path& identityFolder)
{
    Formats::BootMiiKeyBlock block = Formats::ReadBootMiiKeyBlock(keyBlock);
    const Formats::OneTimeProgrammable& memory = block.oneTimeProgrammable;
    const Formats::SerialEeprom& eeprom = block.serialEeprom;

    Formats::ConsoleKeys keys = MakeSharedKeys();
    keys.boot1Hash = memory.boot1Hash;
    keys.commonKey = memory.commonKey;
    keys.consoleId = memory.consoleId;
    keys.ellipticCurvePrivateKey = memory.ellipticCurvePrivateKey;
    keys.nandHmacKey = memory.nandHmacKey;
    keys.nandAesKey = memory.nandAesKey;
    keys.randomNumberKey = memory.randomNumberKey;
    keys.ngKeyId = eeprom.ngKeyId;
    keys.ngSignature = eeprom.ngSignature;
    keys.macAddress = macAddress.value_or(MakeNewMacAddress());

    // The console names the signer of its certificate after the two numbers of its SEEPROM: Root-CA00000001-MS00000002.
    Formats::EllipticCurveCertificateFields device;
    device.issuer = std::format("{}-CA{:08x}-MS{:08x}", RootName, eeprom.certificateAuthorityId, eeprom.manufacturingSignerId);
    device.name = Formats::DeviceCertificateName(keys.consoleId);
    device.keyId = keys.ngKeyId;
    device.publicKey = Formats::DerivePublicKey(keys.ellipticCurvePrivateKey);
    device.signature.assign(eeprom.ngSignature.begin(), eeprom.ngSignature.end());

    Formats::ByteView serialEeprom = keyBlock.subspan(SerialEepromOffsetInKeyBlock, Formats::SerialEepromSize);
    WriteIdentityFiles(identityFolder, keys, Formats::WriteEllipticCurveCertificate(device), serialEeprom);
}

}
