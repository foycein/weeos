#pragma once

#include "Media/JpegBitReader.h"
#include "Media/JpegFrame.h"
#include "Media/JpegHuffmanTable.h"

#include <array>

namespace Media
{

constexpr int HuffmanTableSlots = 4;

// The Huffman tables that are defined when a scan starts. A file may define them again between scans.
struct JpegHuffmanTables
{
    std::array<JpegHuffmanTable, HuffmanTableSlots> dc;
    std::array<JpegHuffmanTable, HuffmanTableSlots> ac;
};

// Decodes the coefficients of one scan into the blocks of the frame's components.
//
// A baseline picture has one scan with every coefficient of every block. A progressive picture (Annex G) sends the
// coefficients in several scans: first the averages (DC) of all blocks, then bands of the other coefficients (AC), and
// each first with its high bits only and later with the bits below them, one bit per refining scan.
class JpegScanDecoder
{
public:
    JpegScanDecoder(JpegFrame& frame, const JpegScan& scan, const JpegHuffmanTables& tables, int restartInterval);

    void Decode(JpegBitReader& bits);

private:
    void DecodeOneComponent(JpegBitReader& bits);
    void DecodeInterleaved(JpegBitReader& bits);
    void RestartIfDue(JpegBitReader& bits, int& unitsUntilRestart);
    void DecodeBlock(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits);

    void DecodeWholeBlock(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits);
    void DecodeFirstAverage(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits);
    void RefineAverage(CoefficientBlock& block, JpegBitReader& bits);
    void DecodeFirstBand(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits);
    void RefineBand(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits);
    int RefineCodedCoefficients(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits);
    int SkipZeroCoefficients(CoefficientBlock& block, int zigZagPosition, int zerosToSkip, JpegBitReader& bits);
    void RefineNonZeroCoefficient(std::int16_t& coefficient, JpegBitReader& bits);

    int ReadAverageDifference(JpegComponent& component, JpegBitReader& bits);
    const JpegHuffmanTable& DcTableOf(const JpegComponent& component) const;
    const JpegHuffmanTable& AcTableOf(const JpegComponent& component) const;

    JpegFrame& frame;
    const JpegScan& scan;
    const JpegHuffmanTables& tables;
    int restartInterval;

    // How many more blocks of a band hold nothing but zeros (an end-of-band run of a progressive scan).
    int endOfBandRun = 0;
};

}
