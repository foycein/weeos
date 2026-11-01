#include "SharpArt/VectorCanvas.h"

#include <algorithm>
#include <cmath>

namespace SharpArt
{

namespace
{

// Every row of pixels is measured at this many heights; across a row the coverage is exact.
constexpr int SamplesPerRow = 16;

constexpr float FullByte = 255;

float ByteToShare(Formats::Byte value)
{
    return value / FullByte;
}

Formats::Byte ShareToByte(float share)
{
    return static_cast<Formats::Byte>(std::lround(std::clamp(share, 0.0f, 1.0f) * FullByte));
}

// Where the edge crosses the height y, when it does. An edge counts from its upper end and not its lower one, so two edges that
// meet at a corner are counted once.
bool CrossingAt(Point from, Point to, float y, float& crossingX)
{
    float upper = std::min(from.y, to.y);
    float lower = std::max(from.y, to.y);
    if (y < upper || y >= lower)
        return false;

    float share = (y - from.y) / (to.y - from.y);
    crossingX = from.x + (to.x - from.x) * share;
    return true;
}

// Adds the share of every pixel of the row that the span from start to end covers, times the weight.
void AddSpan(std::vector<float>& rowCoverage, float spanStart, float spanEnd, float weight)
{
    auto firstColumn = static_cast<std::size_t>(std::floor(spanStart));
    for (std::size_t column = firstColumn; column < rowCoverage.size(); ++column)
    {
        float columnStart = static_cast<float>(column);
        if (columnStart >= spanEnd)
            break;

        float covered = std::min(spanEnd, columnStart + 1) - std::max(spanStart, columnStart);
        rowCoverage[column] += covered * weight;
    }
}

// Paints the source color over the color that is there, with the source alpha times the coverage.
Formats::Rgba8Pixel PaintOver(Formats::Rgba8Pixel below, Color source, float coverage)
{
    float sourceAlpha = ByteToShare(source.alpha) * coverage;
    float belowAlpha = ByteToShare(below.alpha);
    float resultAlpha = sourceAlpha + belowAlpha * (1 - sourceAlpha);
    if (resultAlpha <= 0)
        return below;

    auto mix = [&](Formats::Byte sourceValue, Formats::Byte belowValue)
    {
        float mixed = ByteToShare(sourceValue) * sourceAlpha + ByteToShare(belowValue) * belowAlpha * (1 - sourceAlpha);
        return ShareToByte(mixed / resultAlpha);
    };

    Formats::Rgba8Pixel result;
    result.red = mix(source.red, below.red);
    result.green = mix(source.green, below.green);
    result.blue = mix(source.blue, below.blue);
    result.alpha = ShareToByte(resultAlpha);
    return result;
}

}

VectorCanvas::VectorCanvas(std::uint32_t width, std::uint32_t height, std::uint32_t pixelsPerUnit, Color transparentColor)
    : unitWidth(width)
    , unitHeight(height)
    , scale(static_cast<float>(pixelsPerUnit))
{
    picture.width = width * pixelsPerUnit;
    picture.height = height * pixelsPerUnit;
    Formats::Rgba8Pixel emptyPixel = transparentColor;
    emptyPixel.alpha = 0;
    picture.pixels.assign(static_cast<std::size_t>(picture.width) * picture.height, emptyPixel);
    rowCoverage.assign(picture.width, 0);
}

std::uint32_t VectorCanvas::UnitWidth() const
{
    return unitWidth;
}

std::uint32_t VectorCanvas::UnitHeight() const
{
    return unitHeight;
}

void VectorCanvas::Fill(const VectorPath& path, const Paint& paint)
{
    std::vector<Edge> edges = EdgesInPixels(path);
    for (std::uint32_t row = 0; row < picture.height; ++row)
        FillRow(edges, row, paint);
}

const Formats::RgbaImage& VectorCanvas::Picture() const
{
    return picture;
}

Formats::RgbaImage& VectorCanvas::Picture()
{
    return picture;
}

std::vector<VectorCanvas::Edge> VectorCanvas::EdgesInPixels(const VectorPath& path) const
{
    std::vector<Edge> edges;
    for (const Outline& outline : path.Outlines())
    {
        for (std::size_t index = 0; index < outline.size(); ++index)
        {
            Point from = outline[index];
            Point to = outline[(index + 1) % outline.size()];
            if (from.y == to.y)
                continue;

            edges.push_back({{from.x * scale, from.y * scale}, {to.x * scale, to.y * scale}});
        }
    }
    return edges;
}

// Measures how much of every pixel of the row the shape covers, then paints the pixels it touches.
void VectorCanvas::FillRow(const std::vector<Edge>& edges, std::uint32_t row, const Paint& paint)
{
    std::fill(rowCoverage.begin(), rowCoverage.end(), 0.0f);
    float rowWidth = static_cast<float>(picture.width);
    constexpr float sampleWeight = 1.0f / SamplesPerRow;
    std::vector<float> crossings;

    for (int sample = 0; sample < SamplesPerRow; ++sample)
    {
        float y = static_cast<float>(row) + (static_cast<float>(sample) + 0.5f) * sampleWeight;

        crossings.clear();
        for (const Edge& edge : edges)
        {
            float crossingX = 0;
            if (CrossingAt(edge.from, edge.to, y, crossingX))
                crossings.push_back(crossingX);
        }
        std::sort(crossings.begin(), crossings.end());

        // Between the first and the second crossing the line is inside, between the second and the third outside, and so on.
        for (std::size_t index = 0; index + 1 < crossings.size(); index += 2)
        {
            float spanStart = std::clamp(crossings[index], 0.0f, rowWidth);
            float spanEnd = std::clamp(crossings[index + 1], 0.0f, rowWidth);
            AddSpan(rowCoverage, spanStart, spanEnd, sampleWeight);
        }
    }

    for (std::uint32_t column = 0; column < picture.width; ++column)
    {
        if (rowCoverage[column] > 0)
            PaintPixel(column, row, rowCoverage[column], paint);
    }
}

void VectorCanvas::PaintPixel(std::uint32_t column, std::uint32_t row, float coverage, const Paint& paint)
{
    Point middleInUnits = {(static_cast<float>(column) + 0.5f) / scale, (static_cast<float>(row) + 0.5f) / scale};
    Color color = paint.ColorAt(middleInUnits);

    Formats::Rgba8Pixel& pixel = picture.pixels[static_cast<std::size_t>(row) * picture.width + column];
    pixel = PaintOver(pixel, color, std::min(coverage, 1.0f));
}

}
