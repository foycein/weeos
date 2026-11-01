#include "SystemMenu/ZoomedWindow.h"

namespace SystemMenu
{

namespace
{

constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};

void DrawBlackAround(Display& display, const PictureArea& television, std::uint8_t alpha)
{
    Formats::Rgba8Color black = Black;
    black.alpha = alpha;

    PictureArea whole = display.WholePicture();
    display.FillArea({whole.left, television.left, whole.bottom, whole.top}, black);
    display.FillArea({television.right, whole.right, whole.bottom, whole.top}, black);
    display.FillArea({television.left, television.right, television.top, whole.top}, black);
    display.FillArea({television.left, television.right, whole.bottom, television.bottom}, black);
}

}

// The original draws the window into a texture and shows the texture (pass 0 and 1). WeeOS draws the window straight into the
// television, every quad as transparent as the zoom says, so parts of the window that lie on each other show through a little more
// while it fades in. Like the texture, the window ends at the edges of the television: a banner reaching past them is cut off.
void DrawWindowInsideTheTelevision(Display& display, const TelevisionZoom& zoom, const std::function<void()>& drawWindow)
{
    PictureArea television = zoom.TelevisionInPicture();
    display.Device().SetScissor(television);
    display.MoveArea(display.WholePicture(), television, zoom.WindowAlpha());
    drawWindow();
    display.StopMovingAreas();
    display.Device().ClearScissor();
    DrawBlackAround(display, television, zoom.WindowAlpha());
}

}
