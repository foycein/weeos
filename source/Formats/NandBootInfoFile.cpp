#include "Formats/NandBootInfoFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t WordSize = 4;
constexpr std::size_t ChecksumSize = WordSize;

// The fields at the start of the file. The menu leaves the bytes between them zero.
constexpr std::size_t ApplicationTypeOffset = 0x0A;
constexpr std::size_t UnusedBytesBeforeLauncher = 8;
constexpr std::size_t ArgumentsStart = 0x20;

// The menu builds the file inside a buffer of 0x2000 bytes, at 0xFE0, and the places of the arguments are counted from the start of that
// buffer. In the file a place is this much smaller.
constexpr std::size_t BufferStartDistance = 0xFE0;

std::uint32_t SumOfWords(ByteView words)
{
    ByteReader reader(words, "NANDBOOTINFO");

    std::uint32_t sum = 0;
    while (!reader.IsAtEnd())
        sum += reader.ReadUint32();

    return sum;
}

void StoreBytes(Bytes& file, std::size_t offset, ByteView bytes)
{
    std::copy(bytes.begin(), bytes.end(), file.begin() + offset);
}

// The place in the file of what the menu counts from the start of its buffer.
std::size_t PlaceInFile(std::uint32_t placeInBuffer, std::string_view what)
{
    bool isInside = placeInBuffer >= ArgumentsStart + BufferStartDistance && placeInBuffer < NandBootInfoFileSize + BufferStartDistance;
    if (!isInside)
        throw FormatError(std::format("NANDBOOTINFO: {} is at {:#x}, outside the arguments", what, placeInBuffer));

    return placeInBuffer - BufferStartDistance;
}

std::vector<std::string> ReadArguments(ByteView bytes, std::uint32_t listPlaceInBuffer)
{
    if (listPlaceInBuffer == 0)
        return {};

    ByteReader listReader(bytes, "NANDBOOTINFO");
    listReader.MoveTo(PlaceInFile(listPlaceInBuffer, "the list of arguments"));
    std::uint32_t argumentCount = listReader.ReadUint32();

    std::vector<std::string> arguments;
    for (std::uint32_t argumentNumber = 0; argumentNumber < argumentCount; ++argumentNumber)
    {
        ByteReader textReader(bytes, "NANDBOOTINFO");
        textReader.MoveTo(PlaceInFile(listReader.ReadUint32(), "an argument"));
        arguments.push_back(textReader.ReadTextUntilZero());
    }
    return arguments;
}

// Puts the texts at the end of the file, the last one first, and gives the place of each in the buffer of the menu.
// Gives the start of the first text through the last parameter.
std::vector<std::uint32_t> StoreArgumentTexts(Bytes& file, const std::vector<std::string>& arguments, std::size_t& firstTextStart)
{
    std::vector<std::uint32_t> textPlaces(arguments.size());

    firstTextStart = NandBootInfoFileSize;
    for (std::size_t argumentNumber = arguments.size(); argumentNumber > 0; --argumentNumber)
    {
        const std::string& argument = arguments[argumentNumber - 1];
        std::size_t textSize = argument.size() + 1;
        if (firstTextStart < ArgumentsStart + textSize)
            throw FormatError("NANDBOOTINFO: the arguments do not fit in the 0x1000 bytes of the file");

        firstTextStart -= textSize;
        std::copy(argument.begin(), argument.end(), file.begin() + firstTextStart);
        textPlaces[argumentNumber - 1] = static_cast<std::uint32_t>(firstTextStart + BufferStartDistance);
    }
    return textPlaces;
}

// Writes how many arguments there are, then the place of each, in front of the texts on a word boundary.
// Gives the place of the count in the buffer of the menu.
std::uint32_t StoreArgumentList(Bytes& file, const std::vector<std::uint32_t>& textPlaces, std::size_t firstTextStart)
{
    std::size_t placeAfterTheList = firstTextStart - firstTextStart % WordSize;
    std::size_t listSize = (textPlaces.size() + 1) * WordSize;
    if (placeAfterTheList < ArgumentsStart + WordSize + listSize)
        throw FormatError("NANDBOOTINFO: the list of arguments does not fit in the 0x1000 bytes of the file");

    std::size_t countPlace = placeAfterTheList - listSize - WordSize;

    ByteWriter list;
    list.WriteUint32(static_cast<std::uint32_t>(textPlaces.size()));
    for (std::uint32_t textPlace : textPlaces)
        list.WriteUint32(textPlace);

    StoreBytes(file, countPlace, list.GetBytes());
    return static_cast<std::uint32_t>(countPlace + BufferStartDistance);
}

}

NandBootInfo ReadNandBootInfo(ByteView bytes)
{
    if (bytes.size() != NandBootInfoFileSize)
        throw FormatError(std::format("NANDBOOTINFO: {} bytes instead of {}", bytes.size(), NandBootInfoFileSize));

    ByteReader reader(bytes, "NANDBOOTINFO");
    std::uint32_t storedSum = reader.ReadUint32();
    std::uint32_t sumOfWords = SumOfWords(bytes.subspan(ChecksumSize));
    if (storedSum != sumOfWords)
        throw FormatError(std::format("NANDBOOTINFO: the sum {:#x} in the file is not the sum {:#x} of its words", storedSum, sumOfWords));

    std::uint32_t listPlaceInBuffer = reader.ReadUint32();

    NandBootInfo info;
    reader.MoveTo(ApplicationTypeOffset);
    info.applicationType = reader.ReadByte();
    info.titleType = reader.ReadByte();
    info.launchCode = reader.ReadUint32();
    reader.Skip(UnusedBytesBeforeLauncher);
    info.launcherTitleId = reader.ReadUint64();
    info.arguments = ReadArguments(bytes, listPlaceInBuffer);
    return info;
}

Bytes WriteNandBootInfo(const NandBootInfo& info)
{
    Bytes file(NandBootInfoFileSize, Byte(0));

    std::uint32_t listPlaceInBuffer = 0;
    if (!info.arguments.empty())
    {
        std::size_t firstTextStart = 0;
        std::vector<std::uint32_t> textPlaces = StoreArgumentTexts(file, info.arguments, firstTextStart);
        listPlaceInBuffer = StoreArgumentList(file, textPlaces, firstTextStart);
    }

    ByteWriter header;
    header.WriteUint32(0);
    header.WriteUint32(listPlaceInBuffer);
    header.WriteUint16(0);
    header.WriteByte(info.applicationType);
    header.WriteByte(info.titleType);
    header.WriteUint32(info.launchCode);
    header.WriteZeros(UnusedBytesBeforeLauncher);
    header.WriteUint64(info.launcherTitleId);
    StoreBytes(file, 0, header.GetBytes());

    ByteWriter sum;
    sum.WriteUint32(SumOfWords(ByteView(file).subspan(ChecksumSize)));
    StoreBytes(file, 0, sum.GetBytes());
    return file;
}

}
