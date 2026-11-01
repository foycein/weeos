#pragma once

#include "Platform/WindowSize.h"

namespace Platform
{

// The edges of a window on the desktop, in pixels, as Windows gives them while the user drags the window's frame.
struct WindowEdges
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    bool operator==(const WindowEdges& other) const = default;
};

// The edge or the corner of the window's frame that the user drags.
enum class DraggedEdge
{
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

// Changes the edges so that the area inside the frame stays 16:9 while the user drags. The frame is how much wider and taller
// the window is than the area inside it. The dragged side decides the size: the width when a side or a corner is dragged, the height
// when the top or the bottom is dragged. The edges that move are the ones across from the edges that stay where they are.
WindowEdges KeepInsideSixteenByNine(WindowEdges edges, DraggedEdge draggedEdge, WindowSize frame);

}
