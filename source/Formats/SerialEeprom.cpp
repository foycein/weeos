#include "Formats/SerialEeprom.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t ManufacturingSignerOffset = 0x00;
constexpr std::size_t CertificateAuthorityOffset = 0x04;
constexpr std::size_t NgKeyIdOffset = 0x08;
constexpr std::size_t NgSignatureOffset = 0x0C;
constexpr std::size_t BootTwoVersionOffset = 0x48;
constexpr std::size_t KoreanKeyOffset = 0x74;

}

SerialEeprom ReadSerialEeprom(ByteView bytes)
{
    if (bytes.size() != SerialEepromSize)
        throw FormatError(std::format("SEEPROM: the data has {} bytes instead of {}", bytes.size(), SerialEepromSize));

    ByteReader reader(bytes, "SEEPROM");

    SerialEeprom eeprom;
    reader.MoveTo(ManufacturingSignerOffset);
    eeprom.manufacturingSignerId = reader.ReadUint32();
    reader.MoveTo(CertificateAuthorityOffset);
    eeprom.certificateAuthorityId = reader.ReadUint32();
    reader.MoveTo(NgKeyIdOffset);
    eeprom.ngKeyId = reader.ReadUint32();
    reader.MoveTo(NgSignatureOffset);
    eeprom.ngSignature = reader.ReadByteArray<NgSignatureSize>();
    reader.MoveTo(BootTwoVersionOffset);
    eeprom.bootTwoVersion = reader.ReadByte();
    reader.MoveTo(KoreanKeyOffset);
    eeprom.koreanKey = reader.ReadByteArray<KoreanKeySize>();
    return eeprom;
}

}
