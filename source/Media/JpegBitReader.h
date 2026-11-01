#pragma once

#include "Formats/Bytes.h"

#include <cstddef>

namespace Media
{

// Reads the entropy coded data of one scan bit by bit, the most significant bit of each byte first.
//
// In a JPEG file a byte 0xFF inside the data is followed by a 0x00 (byte stuffing), so that it cannot be taken for a marker.
// The reader drops that 0x00. At a marker the data of the scan is over and the reader gives zero bits from then on, so a picture
// that ends too early still decodes and shows gray where its data is missing.
class JpegBitReader
{
public:
    explicit JpegBitReader(Formats::ByteView scanData);

    int ReadBit();

    // Reads count bits as an unsigned number, the first bit is the most significant.
    int ReadBits(int count);

    // Reads count bits and turns them into a signed number the way JPEG stores differences and coefficients: a first bit of 0
    // means a negative number (EXTEND of the standard, section F.2.2.1).
    int ReadSignedNumber(int count);

    // A restart marker (RST0 to RST7) ends a restart interval: the bits left in the current byte are padding.
    // Drops them and steps over the marker.
    void Restart();

private:
    int ReadByte();
    bool IsRestartMarkerAt(std::size_t place) const;

    Formats::ByteView data;
    std::size_t position = 0;
    int currentByte = 0;
    int bitsLeftInByte = 0;
};

}
