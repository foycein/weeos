#include "Formats/Huffman.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr Byte HuffmanCompressionType = 2;
constexpr int TypeBit = 4;
constexpr Byte SymbolBitCountMask = 0x0F;
constexpr std::size_t SizeFieldByteCount = 3;
constexpr std::size_t HeaderSize = 4;
constexpr std::size_t BitsPerWord = 32;
constexpr std::size_t BytesPerWord = 4;
constexpr std::size_t RootNodeAddress = 1;

constexpr Byte ChildOffsetMask = 0x3F;
constexpr Byte FirstChildIsSymbolFlag = 0x80;
constexpr Byte SecondChildIsSymbolFlag = 0x40;

constexpr int FourBitSymbols = 4;
constexpr int EightBitSymbols = 8;
constexpr int NibbleBitCount = 4;
constexpr Byte NibbleMask = 0x0F;

// Reads the bits of the compressed stream. The stream is made of words of 32 bits that are stored with
// the least significant byte first. The highest bit of each word comes first.
class BitStream
{
public:
    explicit BitStream(ByteView streamBytes)
        : bytes(streamBytes)
    {
    }

    unsigned ReadBit()
    {
        std::size_t wordIndex = bitPosition / BitsPerWord;
        std::size_t bitInWord = BitsPerWord - 1 - bitPosition % BitsPerWord;
        std::size_t byteIndex = wordIndex * BytesPerWord + bitInWord / BitsPerByte;
        if (byteIndex >= bytes.size())
            throw FormatError("Huffman data: the stream ends before all the data is decoded");

        ++bitPosition;
        return (bytes[byteIndex] >> (bitInWord % BitsPerByte)) & 1;
    }

private:
    ByteView bytes;
    std::size_t bitPosition = 0;
};

// The tree is a table of nodes. The table starts with the byte that tells its size, so the root node has the
// address 1. A node says where its two children are: next to each other, at an offset counted in pairs of bytes.
// Two flags tell whether a child is a symbol that ends the walk through the tree.
class HuffmanTree
{
public:
    explicit HuffmanTree(ByteView tableBytes)
        : table(tableBytes)
    {
    }

    // Follows the bits to a symbol and gives it.
    Byte ReadSymbol(BitStream& stream) const
    {
        std::size_t nodeAddress = RootNodeAddress;

        while (true)
        {
            Byte node = TableByte(nodeAddress);
            unsigned bit = stream.ReadBit();

            std::size_t childAddress = (nodeAddress & ~std::size_t(1)) + (node & ChildOffsetMask) * 2 + 2 + bit;
            Byte childIsSymbolFlag = bit == 0 ? FirstChildIsSymbolFlag : SecondChildIsSymbolFlag;
            if ((node & childIsSymbolFlag) != 0)
                return TableByte(childAddress);

            nodeAddress = childAddress;
        }
    }

private:
    Byte TableByte(std::size_t address) const
    {
        if (address >= table.size())
            throw FormatError("Huffman data: the tree points outside its table");

        return table[address];
    }

    ByteView table;
};

}

Bytes DecompressHuffman(ByteView compressedBytes)
{
    ByteReader reader(compressedBytes, "Huffman data");

    Byte typeAndSymbolSize = reader.ReadByte();
    Byte type = typeAndSymbolSize >> TypeBit;
    int symbolBitCount = typeAndSymbolSize & SymbolBitCountMask;
    if (type != HuffmanCompressionType)
        reader.Fail(std::format("the compression type {} is not the Huffman type {}", type, HuffmanCompressionType));

    if (symbolBitCount != FourBitSymbols && symbolBitCount != EightBitSymbols)
        reader.Fail(std::format("the symbols have {} bits, but only 4 and 8 are known", symbolBitCount));

    std::size_t outputSize = 0;
    for (std::size_t byteIndex = 0; byteIndex < SizeFieldByteCount; ++byteIndex)
        outputSize |= std::size_t(reader.ReadByte()) << (BitsPerByte * byteIndex);

    std::size_t treeSizeInPairs = reader.ReadByte();
    std::size_t tableSize = (treeSizeInPairs + 1) * 2;
    reader.MoveTo(HeaderSize);
    ByteView table = reader.ReadBytes(tableSize);

    HuffmanTree tree(table);
    BitStream stream(compressedBytes.subspan(HeaderSize + tableSize));

    Bytes output;
    output.reserve(outputSize);
    while (output.size() < outputSize)
    {
        Byte symbol = tree.ReadSymbol(stream);
        if (symbolBitCount == EightBitSymbols)
        {
            output.push_back(symbol);
            continue;
        }

        Byte secondSymbol = tree.ReadSymbol(stream);
        output.push_back(static_cast<Byte>((symbol & NibbleMask) | ((secondSymbol & NibbleMask) << NibbleBitCount)));
    }

    return output;
}

}
