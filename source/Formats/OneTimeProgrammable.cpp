#include "Formats/OneTimeProgrammable.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t Boot1HashOffset = 0x00;
constexpr std::size_t CommonKeyOffset = 0x14;
constexpr std::size_t ConsoleIdOffset = 0x24;
constexpr std::size_t EllipticCurvePrivateKeyOffset = 0x28;
constexpr std::size_t NandHmacKeyOffset = 0x44;
constexpr std::size_t NandAesKeyOffset = 0x58;
constexpr std::size_t RandomNumberKeyOffset = 0x68;

}

OneTimeProgrammable ReadOneTimeProgrammable(ByteView bytes)
{
    if (bytes.size() != OneTimeProgrammableSize)
        throw FormatError(std::format("OTP memory: the data has {} bytes instead of {}", bytes.size(), OneTimeProgrammableSize));

    ByteReader reader(bytes, "OTP memory");

    OneTimeProgrammable memory;
    reader.MoveTo(Boot1HashOffset);
    memory.boot1Hash = reader.ReadByteArray<Sha1DigestSize>();
    reader.MoveTo(CommonKeyOffset);
    memory.commonKey = reader.ReadByteArray<AesBlockSize>();
    reader.MoveTo(ConsoleIdOffset);
    memory.consoleId = reader.ReadUint32();
    reader.MoveTo(EllipticCurvePrivateKeyOffset);
    memory.ellipticCurvePrivateKey = reader.ReadByteArray<EllipticCurvePrivateKeySize>();
    reader.MoveTo(NandHmacKeyOffset);
    memory.nandHmacKey = reader.ReadByteArray<NandHmacKeySize>();
    reader.MoveTo(NandAesKeyOffset);
    memory.nandAesKey = reader.ReadByteArray<AesBlockSize>();
    reader.MoveTo(RandomNumberKeyOffset);
    memory.randomNumberKey = reader.ReadByteArray<AesBlockSize>();
    return memory;
}

}
