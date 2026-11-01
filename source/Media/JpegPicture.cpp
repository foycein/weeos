#include "Media/JpegPicture.h"

#include "Media/InverseDct.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Media
{

namespace
{

// The samples of a block are stored around zero; the picture's are from 0 to 255 (level shift, section A.3.1).
constexpr float LevelShift = 128;
constexpr float LargestSample = 255;
constexpr Formats::Byte Opaque = 255;

// The conversion of JFIF from YCbCr to RGB.
constexpr float RedFromCr = 1.402f;
constexpr float GreenFromCb = 0.344136f;
constexpr float GreenFromCr = 0.714136f;
constexpr float BlueFromCb = 1.772f;
constexpr float ColorDifferenceMiddle = 128;

// The samples of one component, as many as its blocks hold (a few more than the picture needs at the right and the bottom).
struct SamplePlane
{
    int width = 0;
    int height = 0;
    std::vector<float> samples;

    float At(int x, int y) const
    {
        return samples[y * width + x];
    }
};

Formats::Byte ToByte(float value)
{
    float clamped = std::clamp(value, 0.0f, LargestSample);
    return static_cast<Formats::Byte>(std::lround(clamped));
}

void PutBlockIntoPlane(const SampleBlock& block, int blockRow, int blockColumn, SamplePlane& plane)
{
    for (int row = 0; row < BlockSide; ++row)
    {
        for (int column = 0; column < BlockSide; ++column)
        {
            float sample = block[row * BlockSide + column] + LevelShift;
            int x = blockColumn * BlockSide + column;
            int y = blockRow * BlockSide + row;
            plane.samples[y * plane.width + x] = std::round(std::clamp(sample, 0.0f, LargestSample));
        }
    }
}

SamplePlane DecodeComponentSamples(const JpegComponent& component, const QuantizationTable& quantization)
{
    SamplePlane plane;
    plane.width = component.blocksPerLine * BlockSide;
    plane.height = component.blocksPerColumn * BlockSide;
    plane.samples.assign(static_cast<std::size_t>(plane.width) * plane.height, LevelShift);

    for (int blockRow = 0; blockRow < component.usedBlocksPerColumn; ++blockRow)
    {
        for (int blockColumn = 0; blockColumn < component.usedBlocksPerLine; ++blockColumn)
        {
            const CoefficientBlock& block = component.BlockAt(blockRow, blockColumn);
            std::array<float, CoefficientsPerBlock> dequantized{};
            for (int index = 0; index < CoefficientsPerBlock; ++index)
                dequantized[index] = static_cast<float>(block[index] * quantization[index]);

            PutBlockIntoPlane(InverseDct(dequantized), blockRow, blockColumn, plane);
        }
    }
    return plane;
}

// Where pixel x of the picture lies among the samples of a component with fewer samples. The samples sit in the middle of the
// pixels they cover, so with half as many samples pixel 0 lies at -0.25 and pixel 1 at 0.25.
float PlaceAmongSamples(int pixel, int sampling, int largestSampling)
{
    return (pixel + 0.5f) * sampling / largestSampling - 0.5f;
}

// The sample of the component at a pixel of the picture, taken between the four nearest samples (bilinear).
float SampleAtPixel(const SamplePlane& plane, const JpegComponent& component, const JpegFrame& frame, int x, int y)
{
    bool hasEverySample = component.horizontalSampling == frame.largestHorizontalSampling
        && component.verticalSampling == frame.largestVerticalSampling;
    if (hasEverySample)
        return plane.At(x, y);

    float placeX = PlaceAmongSamples(x, component.horizontalSampling, frame.largestHorizontalSampling);
    float placeY = PlaceAmongSamples(y, component.verticalSampling, frame.largestVerticalSampling);
    placeX = std::clamp(placeX, 0.0f, static_cast<float>(component.sampleWidth - 1));
    placeY = std::clamp(placeY, 0.0f, static_cast<float>(component.sampleHeight - 1));

    int left = static_cast<int>(placeX);
    int top = static_cast<int>(placeY);
    int right = std::min(left + 1, component.sampleWidth - 1);
    int bottom = std::min(top + 1, component.sampleHeight - 1);
    float towardsRight = placeX - left;
    float towardsBottom = placeY - top;

    float upper = plane.At(left, top) * (1 - towardsRight) + plane.At(right, top) * towardsRight;
    float lower = plane.At(left, bottom) * (1 - towardsRight) + plane.At(right, bottom) * towardsRight;
    return upper * (1 - towardsBottom) + lower * towardsBottom;
}

Formats::Rgba8Pixel ColorOfSamples(float brightness, float blueDifference, float redDifference)
{
    float cb = blueDifference - ColorDifferenceMiddle;
    float cr = redDifference - ColorDifferenceMiddle;

    Formats::Rgba8Pixel pixel{};
    pixel.red = ToByte(brightness + RedFromCr * cr);
    pixel.green = ToByte(brightness - GreenFromCb * cb - GreenFromCr * cr);
    pixel.blue = ToByte(brightness + BlueFromCb * cb);
    pixel.alpha = Opaque;
    return pixel;
}

Formats::Rgba8Pixel GrayPixel(float brightness)
{
    Formats::Byte gray = ToByte(brightness);
    return {gray, gray, gray, Opaque};
}

}

Formats::RgbaImage MakeJpegPicture(const JpegFrame& frame, const std::array<QuantizationTable, QuantizationTableSlots>& quantizationTables,
    JpegColors colors)
{
    std::vector<SamplePlane> planes;
    for (const JpegComponent& component : frame.components)
        planes.push_back(DecodeComponentSamples(component, quantizationTables[component.quantizationTableIndex]));

    Formats::RgbaImage picture;
    picture.width = static_cast<std::uint32_t>(frame.width);
    picture.height = static_cast<std::uint32_t>(frame.height);
    picture.pixels.reserve(static_cast<std::size_t>(frame.width) * frame.height);

    bool isGray = frame.components.size() == 1;
    for (int y = 0; y < frame.height; ++y)
    {
        for (int x = 0; x < frame.width; ++x)
        {
            float first = SampleAtPixel(planes[0], frame.components[0], frame, x, y);
            if (isGray)
            {
                picture.pixels.push_back(GrayPixel(first));
                continue;
            }

            float second = SampleAtPixel(planes[1], frame.components[1], frame, x, y);
            float third = SampleAtPixel(planes[2], frame.components[2], frame, x, y);
            if (colors == JpegColors::RedGreenBlue)
                picture.pixels.push_back({ToByte(first), ToByte(second), ToByte(third), Opaque});
            else
                picture.pixels.push_back(ColorOfSamples(first, second, third));
        }
    }
    return picture;
}

}
