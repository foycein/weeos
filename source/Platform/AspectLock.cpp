#include "Platform/AspectLock.h"

#include "Platform/FitPicture.h"

#include <cmath>

namespace Platform
{

namespace
{

bool HeightDecides(DraggedEdge draggedEdge)
{
    return draggedEdge == DraggedEdge::Top || draggedEdge == DraggedEdge::Bottom;
}

bool TopMoves(DraggedEdge draggedEdge)
{
    return draggedEdge == DraggedEdge::TopLeft || draggedEdge == DraggedEdge::TopRight;
}

int HeightForWidth(int insideWidth)
{
    return static_cast<int>(std::lround(insideWidth * PictureHeightShare / PictureWidthShare));
}

int WidthForHeight(int insideHeight)
{
    return static_cast<int>(std::lround(insideHeight * PictureWidthShare / PictureHeightShare));
}

}

WindowEdges KeepInsideSixteenByNine(WindowEdges edges, DraggedEdge draggedEdge, WindowSize frame)
{
    int frameWidth = static_cast<int>(frame.width);
    int frameHeight = static_cast<int>(frame.height);

    if (HeightDecides(draggedEdge))
    {
        int insideHeight = edges.bottom - edges.top - frameHeight;
        edges.right = edges.left + WidthForHeight(insideHeight) + frameWidth;
        return edges;
    }

    int insideWidth = edges.right - edges.left - frameWidth;
    int windowHeight = HeightForWidth(insideWidth) + frameHeight;
    if (TopMoves(draggedEdge))
        edges.top = edges.bottom - windowHeight;
    else
        edges.bottom = edges.top + windowHeight;

    return edges;
}

}
