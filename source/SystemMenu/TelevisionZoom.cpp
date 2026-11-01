#include "SystemMenu/TelevisionZoom.h"

namespace SystemMenu
{

namespace
{

constexpr float FullAlpha = 255;

// The movement starts and ends slowly: 3t² - 2t³, the curve of the original's movers.
float SlowStartAndEnd(float time)
{
    return time * time * (3 - 2 * time);
}

float Between(float from, float to, float part)
{
    return from + (to - from) * part;
}

}

TelevisionZoom::TelevisionZoom(PictureArea wholePicture, PictureArea televisionArea)
    : picture(wholePicture)
    , television(televisionArea)
{
}

void TelevisionZoom::StartZoomingIn()
{
    step = 0;
    isZoomingIn = true;
}

void TelevisionZoom::StartZoomingOut()
{
    step = 0;
    isZoomingIn = false;
}

void TelevisionZoom::Step()
{
    if (step < StepCount)
        ++step;
}

bool TelevisionZoom::IsMoving() const
{
    return step < StepCount;
}

float TelevisionZoom::Closeness() const
{
    float movement = SlowStartAndEnd(static_cast<float>(step) / StepCount);
    return isZoomingIn ? movement : 1 - movement;
}

PictureArea TelevisionZoom::VisibleArea() const
{
    float closeness = Closeness();
    PictureArea area;
    area.left = Between(picture.left, television.left, closeness);
    area.right = Between(picture.right, television.right, closeness);
    area.bottom = Between(picture.bottom, television.bottom, closeness);
    area.top = Between(picture.top, television.top, closeness);
    return area;
}

Formats::Vector2 TelevisionZoom::PointInPicture(Formats::Vector2 listPoint) const
{
    PictureArea visible = VisibleArea();
    float scaleX = (picture.right - picture.left) / (visible.right - visible.left);
    float scaleY = (picture.top - picture.bottom) / (visible.top - visible.bottom);

    Formats::Vector2 point;
    point.x = picture.left + (listPoint.x - visible.left) * scaleX;
    point.y = picture.bottom + (listPoint.y - visible.bottom) * scaleY;
    return point;
}

PictureArea TelevisionZoom::TelevisionInPicture() const
{
    Formats::Vector2 bottomLeft = PointInPicture({television.left, television.bottom});
    Formats::Vector2 topRight = PointInPicture({television.right, television.top});

    PictureArea area;
    area.left = bottomLeft.x;
    area.bottom = bottomLeft.y;
    area.right = topRight.x;
    area.top = topRight.y;
    return area;
}

std::uint8_t TelevisionZoom::WindowAlpha() const
{
    return static_cast<std::uint8_t>(FullAlpha * Closeness());
}

}
