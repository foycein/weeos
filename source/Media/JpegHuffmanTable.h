#pragma once

#include "Media/JpegBitReader.h"

#include "Formats/Bytes.h"

#include <array>
#include <vector>

namespace Media
{

constexpr int LongestHuffmanCode = 16;

// A Huffman table as the file defines it (DHT): how many codes there are of each length from 1 to 16 bits, and the values
// of the codes, the short codes first.
struct HuffmanTableDefinition
{
    std::array<int, LongestHuffmanCode> codeCountOfLength{};
    std::vector<Formats::Byte> values;
};

// Turns the bits of a scan into the values of a Huffman table. The codes of one length are consecutive numbers, and each
// length continues where the shorter one ended, doubled (Annex C of the JPEG standard), so a code is found by its length
// and its distance from the first code of that length.
class JpegHuffmanTable
{
public:
    JpegHuffmanTable() = default;
    explicit JpegHuffmanTable(const HuffmanTableDefinition& definition);

    bool IsDefined() const;

    // Throws a FormatError when the bits are no code of the table.
    Formats::Byte ReadValue(JpegBitReader& bits) const;

private:
    struct CodesOfOneLength
    {
        int firstCode = 0;
        int count = 0;
        int firstValueIndex = 0;
    };

    std::array<CodesOfOneLength, LongestHuffmanCode + 1> codesOfLength{};
    std::vector<Formats::Byte> values;
};

// The tables of Annex K of the JPEG standard. Motion-JPEG frames of AVI files leave out their Huffman tables and use these.
HuffmanTableDefinition StandardLuminanceDcTable();
HuffmanTableDefinition StandardChrominanceDcTable();
HuffmanTableDefinition StandardLuminanceAcTable();
HuffmanTableDefinition StandardChrominanceAcTable();

}
