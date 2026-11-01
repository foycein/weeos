#include "Formats/StateFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"
#include "Formats/WordSum.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t ChecksumSize = 4;
constexpr std::string_view Description = "State file";

}

StateFile ReadStateFile(ByteView bytes)
{
    if (bytes.size() != StateFileSize)
        throw FormatError(std::format("State file: {} bytes instead of {}", bytes.size(), StateFileSize));

    ByteReader reader(bytes, Description);
    std::uint32_t storedSum = reader.ReadUint32();
    std::uint32_t sumOfWords = SumOfWords(bytes.subspan(ChecksumSize), Description);
    if (storedSum != sumOfWords)
        throw FormatError(std::format("State file: the sum {:#x} in the file is not the sum {:#x} of its words", storedSum, sumOfWords));

    StateFile state;
    state.flags = reader.ReadByte();
    state.type = reader.ReadByte();
    state.discState = reader.ReadByte();
    state.returnTo = reader.ReadByte();
    state.spare = reader.ReadByteArray<StateFileSpareSize>();
    return state;
}

Bytes WriteStateFile(const StateFile& state)
{
    ByteWriter words;
    words.WriteByte(state.flags);
    words.WriteByte(state.type);
    words.WriteByte(state.discState);
    words.WriteByte(state.returnTo);
    words.WriteBytes(state.spare);

    ByteWriter file;
    file.WriteUint32(SumOfWords(words.GetBytes(), Description));
    file.WriteBytes(words.GetBytes());
    return file.GetBytes();
}

}
