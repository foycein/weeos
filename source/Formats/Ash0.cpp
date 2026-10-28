#include "Formats/Ash0.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <cstdint>
#include <format>
#include <vector>

namespace Formats
{

namespace
{

constexpr std::uint32_t Tag = 0x41534830;
constexpr std::size_t HeaderSize = 12;
constexpr std::uint32_t SizeMask = 0x00FFFFFF;
constexpr int SymbolBitCount = 9;
constexpr int DistanceBitCount = 11;
constexpr std::uint32_t FirstLengthSymbol = 0x100;
constexpr std::size_t ShortestMatchLength = 3;
constexpr int NoNode = -1;

// Reads single bits, the highest bit of each byte first, from one part of the compressed data.
class BitReader
{
public:
    explicit BitReader(ByteView streamBytes)
        : bytes(streamBytes)
    {
    }

    unsigned ReadBit()
    {
        std::size_t byteIndex = bitPosition / BitsPerByte;
        if (byteIndex >= bytes.size())
            throw FormatError("ASH0 data: a stream ends before all the data is decoded");

        int bitIndex = static_cast<int>(BitsPerByte - 1 - bitPosition % BitsPerByte);
        ++bitPosition;
        return (bytes[byteIndex] >> bitIndex) & 1;
    }

    std::uint32_t ReadBits(int bitCount)
    {
        std::uint32_t value = 0;
        for (int bit = 0; bit < bitCount; ++bit)
            value = (value << 1) | ReadBit();
        return value;
    }

private:
    ByteView bytes;
    std::size_t bitPosition = 0;
};

struct TreeNode
{
    int leftChild = NoNode;
    int rightChild = NoNode;
    std::uint32_t leafValue = 0;
    bool isLeaf = false;
};

// A Huffman tree that is stored in front of the data. A 1 bit starts a node with two subtrees, a 0 bit starts
// a leaf that is followed by its value. The tree is written node by node, the left subtree before the right one.
class Tree
{
public:
    Tree(BitReader& reader, int leafBitCount)
    {
        std::vector<int> nodesWaitingForChildren;
        int rootIndex = AddNode(reader, leafBitCount);
        if (nodes[rootIndex].isLeaf)
            return;

        nodesWaitingForChildren.push_back(rootIndex);
        while (!nodesWaitingForChildren.empty())
        {
            int parentIndex = nodesWaitingForChildren.back();
            int childIndex = AddNode(reader, leafBitCount);

            if (nodes[parentIndex].leftChild == NoNode)
            {
                nodes[parentIndex].leftChild = childIndex;
            }
            else
            {
                nodes[parentIndex].rightChild = childIndex;
                nodesWaitingForChildren.pop_back();
            }

            if (!nodes[childIndex].isLeaf)
                nodesWaitingForChildren.push_back(childIndex);
        }
    }

    // Follows the bits of the data from the root to a leaf and gives the value of the leaf.
    std::uint32_t ReadValue(BitReader& reader) const
    {
        int nodeIndex = 0;
        while (!nodes[nodeIndex].isLeaf)
        {
            bool goesRight = reader.ReadBit() != 0;
            nodeIndex = goesRight ? nodes[nodeIndex].rightChild : nodes[nodeIndex].leftChild;
        }
        return nodes[nodeIndex].leafValue;
    }

private:
    int AddNode(BitReader& reader, int leafBitCount)
    {
        TreeNode node;
        node.isLeaf = reader.ReadBit() == 0;
        if (node.isLeaf)
            node.leafValue = reader.ReadBits(leafBitCount);

        nodes.push_back(node);
        return static_cast<int>(nodes.size()) - 1;
    }

    std::vector<TreeNode> nodes;
};

void CopyMatch(Bytes& output, std::size_t distance, std::size_t length, std::size_t outputSize)
{
    if (distance > output.size())
        throw FormatError("ASH0 data: a match copies from before the start of the data");

    for (std::size_t copied = 0; copied < length && output.size() < outputSize; ++copied)
        output.push_back(output[output.size() - distance]);
}

}

bool IsAsh0Compressed(ByteView bytes)
{
    if (bytes.size() < sizeof(Tag))
        return false;

    ByteReader reader(bytes, "ASH0 data");
    return reader.ReadUint32() == Tag;
}

// The data has two streams. The first holds the literal bytes and the lengths of matches, the second
// the distances of matches. Both start with a Huffman tree. The header gives the size of the result
// and where the second stream starts.
Bytes DecompressAsh0(ByteView compressedBytes)
{
    ByteReader header(compressedBytes, "ASH0 data");
    std::uint32_t tag = header.ReadUint32();
    if (tag != Tag)
        header.Fail(std::format("the tag {:#x} is not the ASH0 tag {:#x}", tag, Tag));

    std::uint32_t sizeWord = header.ReadUint32();
    if (sizeWord > SizeMask)
        header.Fail(std::format("the size field {:#x} has unknown bits set", sizeWord));

    std::size_t outputSize = sizeWord;
    std::uint32_t distanceStreamOffset = header.ReadUint32();
    if (distanceStreamOffset < HeaderSize || distanceStreamOffset > compressedBytes.size())
        header.Fail(std::format("the second stream starts at {:#x}, which is outside the data", distanceStreamOffset));

    BitReader symbolReader(compressedBytes.subspan(HeaderSize, distanceStreamOffset - HeaderSize));
    BitReader distanceReader(compressedBytes.subspan(distanceStreamOffset));
    Tree symbolTree(symbolReader, SymbolBitCount);
    Tree distanceTree(distanceReader, DistanceBitCount);

    Bytes output;
    output.reserve(outputSize);
    while (output.size() < outputSize)
    {
        std::uint32_t symbol = symbolTree.ReadValue(symbolReader);
        if (symbol < FirstLengthSymbol)
        {
            output.push_back(static_cast<Byte>(symbol));
            continue;
        }

        std::size_t length = symbol - FirstLengthSymbol + ShortestMatchLength;
        std::size_t distance = distanceTree.ReadValue(distanceReader) + 1;
        CopyMatch(output, distance, length, outputSize);
    }

    return output;
}

}
