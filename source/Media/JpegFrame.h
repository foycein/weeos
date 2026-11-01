#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Media
{

// A JPEG picture is cut into blocks of 8 x 8 samples. Each block is stored as 64 numbers: how strongly each of 64 cosine
// patterns is present (the coefficients). Here a block holds them row by row, the first one is the average of the block.
constexpr int BlockSide = 8;
constexpr int CoefficientsPerBlock = BlockSide * BlockSide;
using CoefficientBlock = std::array<std::int16_t, CoefficientsPerBlock>;

// The file lists the coefficients of a block in a zig-zag order, from the low patterns to the high ones.
// The entry at a zig-zag position is the place of that coefficient in the block, row by row.
extern const std::array<int, CoefficientsPerBlock> ZigZagToRowByRow;

// The quantization table of a component, row by row like a block: the decoded number times the entry is the coefficient.
using QuantizationTable = std::array<std::uint16_t, CoefficientsPerBlock>;

// One color channel of the picture (brightness Y, the colors Cb and Cr, or the red, green and blue of an RGB picture).
// A component can have fewer samples than the picture has pixels: two of its sampling factors against the largest ones of the frame
// mean half as many samples in that direction.
struct JpegComponent
{
    Formats::Byte id = 0;
    int horizontalSampling = 1;
    int verticalSampling = 1;
    int quantizationTableIndex = 0;

    // The samples this component really has, and the blocks that hold them.
    int sampleWidth = 0;
    int sampleHeight = 0;
    int usedBlocksPerLine = 0;
    int usedBlocksPerColumn = 0;

    // The blocks that whole MCUs cover; a block row of the picture may hold a few blocks more than the samples need.
    int blocksPerLine = 0;
    int blocksPerColumn = 0;
    std::vector<CoefficientBlock> blocks;

    // While a scan is decoded: the tables it uses and the last brightness average, which the next block's average is added to.
    int dcTableIndex = 0;
    int acTableIndex = 0;
    int dcPrediction = 0;

    CoefficientBlock& BlockAt(int blockRow, int blockColumn)
    {
        return blocks[blockRow * blocksPerLine + blockColumn];
    }

    const CoefficientBlock& BlockAt(int blockRow, int blockColumn) const
    {
        return blocks[blockRow * blocksPerLine + blockColumn];
    }
};

// The frame header (SOF): the size of the picture and its components.
// An MCU (minimum coded unit) is the group of blocks that an interleaved scan sends together: for each component its
// horizontal sampling times its vertical sampling blocks.
struct JpegFrame
{
    bool isProgressive = false;
    int width = 0;
    int height = 0;
    int largestHorizontalSampling = 1;
    int largestVerticalSampling = 1;
    int mcusPerLine = 0;
    int mcusPerColumn = 0;
    std::vector<JpegComponent> components;
};

// The scan header (SOS): which components follow and, in a progressive picture, which part of their coefficients.
struct JpegScan
{
    std::vector<int> componentIndexes;
    int spectralStart = 0;
    int spectralEnd = CoefficientsPerBlock - 1;

    // Progressive pictures send the high bits of the coefficients first. The scan refines the bits from high down to low
    // (successive approximation); a first scan has a high of 0.
    int successiveHigh = 0;
    int successiveLow = 0;
};

}
