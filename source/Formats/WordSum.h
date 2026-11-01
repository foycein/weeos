#pragma once

#include "Formats/ByteReader.h"
#include "Formats/Bytes.h"

#include <cstdint>
#include <string_view>

namespace Formats
{

// The sum of the 32 bit words, modulo 2^32. The small files of the System Menu start with the sum of the words behind it.
inline std::uint32_t SumOfWords(ByteView words, std::string_view dataDescription)
{
    ByteReader reader(words, dataDescription);

    std::uint32_t sum = 0;
    while (!reader.IsAtEnd())
        sum += reader.ReadUint32();

    return sum;
}

}
