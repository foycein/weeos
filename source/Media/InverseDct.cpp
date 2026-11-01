#include "Media/InverseDct.h"

#include <cmath>
#include <numbers>

namespace Media
{

namespace
{

using CosineTable = std::array<std::array<float, BlockSide>, BlockSide>;

// The weight of frequency u at sample x: C(u) / 2 * cos((2x + 1) * u * pi / 16), with C(0) = 1 / sqrt(2) and C(u) = 1 otherwise.
CosineTable MakeCosineTable()
{
    CosineTable table{};
    for (int sample = 0; sample < BlockSide; ++sample)
    {
        for (int frequency = 0; frequency < BlockSide; ++frequency)
        {
            double scale = frequency == 0 ? 1.0 / std::numbers::sqrt2 : 1.0;
            double angle = (2 * sample + 1) * frequency * std::numbers::pi / (2 * BlockSide);
            table[sample][frequency] = static_cast<float>(scale / 2 * std::cos(angle));
        }
    }
    return table;
}

const CosineTable Cosines = MakeCosineTable();

// One dimension: eight frequencies in, eight samples out. The values lie step apart in the block.
void TransformLine(const float* frequencies, float* samples, int step)
{
    for (int sample = 0; sample < BlockSide; ++sample)
    {
        float sum = 0;
        for (int frequency = 0; frequency < BlockSide; ++frequency)
            sum += Cosines[sample][frequency] * frequencies[frequency * step];

        samples[sample * step] = sum;
    }
}

}

SampleBlock InverseDct(const std::array<float, CoefficientsPerBlock>& coefficients)
{
    std::array<float, CoefficientsPerBlock> rowsDone{};
    for (int row = 0; row < BlockSide; ++row)
        TransformLine(&coefficients[row * BlockSide], &rowsDone[row * BlockSide], 1);

    SampleBlock samples{};
    for (int column = 0; column < BlockSide; ++column)
        TransformLine(&rowsDone[column], &samples[column], BlockSide);

    return samples;
}

}
