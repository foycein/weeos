#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/Vectors.h"

#include <cstdint>

namespace SystemMenu
{

// A rectangle of the picture, in layout units.
using PictureArea = Graphics::ScissorRectangle;

// The zoom between a list of channels and the window of one channel (the channel grid 0x813AE380 and 0x813AE730, the SD Card Menu
// 0x813E0BEC). The list is seen through a camera whose view moves from the whole picture to the television of the channel, or back,
// in 28 steps that start and end slowly. In the same steps the window of the channel fades in inside the television and black covers
// everything around it (0x813B5A54), or the other way round.
class TelevisionZoom
{
public:
    static constexpr int StepCount = 28;

    // The whole picture and the television, in layout units. The zoom stands still, at the whole picture, until it is started.
    TelevisionZoom(PictureArea picture, PictureArea television);

    // From the whole picture to the television, and back.
    void StartZoomingIn();
    void StartZoomingOut();

    void Step();
    bool IsMoving() const;

    // 0 shows the whole list, 1 only the television, which then fills the picture.
    float Closeness() const;

    // How many steps the view has come from the whole picture towards the television: 0 at the whole picture, StepCount at the
    // television. The animations that go with the zoom show this frame of their part (the darkening of the channel grid).
    int StepsTowardsTheTelevision() const;

    // The part of the list that fills the picture now.
    PictureArea VisibleArea() const;

    // Where a point of the list lies in the picture now.
    Formats::Vector2 PointInPicture(Formats::Vector2 listPoint) const;

    // Where the television lies in the picture now. The window of the channel is drawn there.
    PictureArea TelevisionInPicture() const;

    // How much the window of the channel and the black around the television show: 0 not at all, 255 fully.
    std::uint8_t WindowAlpha() const;

private:
    PictureArea picture;
    PictureArea television;
    int step = StepCount;
    bool isZoomingIn = false;
};

}
