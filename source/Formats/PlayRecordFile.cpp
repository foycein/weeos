#include "Formats/PlayRecordFile.h"

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
constexpr std::string_view Description = "Play record";

Bytes WriteWordsBehindTheSum(const PlayRecord& record)
{
    ByteWriter words;
    for (char16_t character : record.name)
        words.WriteUint16(character);

    words.WriteUint64(record.startTime);
    words.WriteUint64(record.lastTime);
    words.WriteBytes(record.gameCode);
    words.WriteBytes(record.makerCode);
    words.WriteBytes(record.spare);
    return words.GetBytes();
}

}

PlayRecord ReadPlayRecord(ByteView bytes)
{
    if (bytes.size() != PlayRecordFileSize)
        throw FormatError(std::format("Play record: {} bytes instead of {}", bytes.size(), PlayRecordFileSize));

    ByteReader reader(bytes, Description);
    std::uint32_t storedSum = reader.ReadUint32();
    std::uint32_t sumOfWords = SumOfWords(bytes.subspan(ChecksumSize), Description);
    if (storedSum != sumOfWords)
        throw FormatError(std::format("Play record: the sum {:#x} in the file is not the sum {:#x} of its words", storedSum, sumOfWords));

    PlayRecord record;
    for (char16_t& character : record.name)
        character = reader.ReadUint16();

    record.startTime = reader.ReadUint64();
    record.lastTime = reader.ReadUint64();
    record.gameCode = reader.ReadByteArray<PlayRecordGameCodeSize>();
    record.makerCode = reader.ReadByteArray<PlayRecordMakerCodeSize>();
    record.spare = reader.ReadByteArray<PlayRecordSpareSize>();
    return record;
}

Bytes WritePlayRecord(const PlayRecord& record)
{
    Bytes words = WriteWordsBehindTheSum(record);

    ByteWriter file;
    file.WriteUint32(SumOfWords(words, Description));
    file.WriteBytes(words);
    return file.GetBytes();
}

}
