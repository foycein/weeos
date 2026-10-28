#pragma once

#include "Formats/Layout.h"
#include "Graphics/GraphicsDevice.h"

#include <array>
#include <cstddef>

namespace Layout
{

// The layout file lists the four corners of a picture as left top, right top, left bottom, right bottom.
// The graphics device wants them going around the quad: left top, right top, right bottom, left bottom.
constexpr std::array<std::size_t, Graphics::QuadCornerCount> FileCornerOfQuadCorner = {0, 1, 3, 2};

template <typename Item>
std::array<Item, Graphics::QuadCornerCount> InQuadOrder(const std::array<Item, Formats::CornerCount>& itemsInFileOrder)
{
    std::array<Item, Graphics::QuadCornerCount> itemsInQuadOrder;
    for (std::size_t quadCorner = 0; quadCorner < Graphics::QuadCornerCount; ++quadCorner)
        itemsInQuadOrder[quadCorner] = itemsInFileOrder[FileCornerOfQuadCorner[quadCorner]];

    return itemsInQuadOrder;
}

}
