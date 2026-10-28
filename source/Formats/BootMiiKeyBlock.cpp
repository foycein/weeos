#include "Formats/BootMiiKeyBlock.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/NandImage.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t HeaderSize = 256;
constexpr std::size_t OneTimeProgrammableOffset = 0x100;
constexpr std::size_t SerialEepromOffset = 0x200;

}

BootMiiKeyBlock ReadBootMiiKeyBlock(ByteView bytes)
{
    if (bytes.size() != NandKeyBlockSize)
        throw FormatError(std::format("BootMii key block: the data has {} bytes instead of {}", bytes.size(), NandKeyBlockSize));

    ByteReader reader(bytes, "BootMii key block");

    BootMiiKeyBlock keyBlock;
    keyBlock.header = reader.ReadText(HeaderSize);
    if (keyBlock.header.ends_with('\n'))
        keyBlock.header.pop_back();

    reader.MoveTo(OneTimeProgrammableOffset);
    keyBlock.oneTimeProgrammable = ReadOneTimeProgrammable(reader.ReadBytes(OneTimeProgrammableSize));
    reader.MoveTo(SerialEepromOffset);
    keyBlock.serialEeprom = ReadSerialEeprom(reader.ReadBytes(SerialEepromSize));
    return keyBlock;
}

}
