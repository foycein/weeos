#pragma once

#include "SharpArt/Paint.h"
#include "SharpArt/VectorPath.h"

#include "Formats/RgbaImage.h"

#include <cstdint>
#include <vector>

namespace SharpArt
{

// A picture that shapes are painted into, one over the other, with smooth edges.
// The shapes are given in the units of the original picture, and every unit becomes pixelsPerUnit pixels.
// The picture starts transparent, in the transparent color; its colors are not multiplied by their alpha, like the pictures of the layouts.
class VectorCanvas
{
public:
    VectorCanvas(std::uint32_t unitWidth, std::uint32_t unitHeight, std::uint32_t pixelsPerUnit, Color transparentColor = {0, 0, 0, 0});

    std::uint32_t UnitWidth() const;
    std::uint32_t UnitHeight() const;

    // Paints the inside of the path over what is there.
    void Fill(const VectorPath& path, const Paint& paint);

    const Formats::RgbaImage& Picture() const;
    Formats::RgbaImage& Picture();

private:
    struct Edge
    {
        Point from;
        Point to;
    };

    std::vector<Edge> EdgesInPixels(const VectorPath& path) const;
    static std::vector<Edge> EdgesTouchingRow(const std::vector<Edge>& edges, std::uint32_t row);
    void FillRow(const std::vector<Edge>& edges, std::uint32_t row, const Paint& paint);
    void PaintPixel(std::uint32_t column, std::uint32_t row, float coverage, const Paint& paint);

    std::uint32_t unitWidth;
    std::uint32_t unitHeight;
    float scale;
    Formats::RgbaImage picture;
    std::vector<float> rowCoverage;
};

}
