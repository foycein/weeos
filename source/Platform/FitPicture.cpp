#include "Platform/FitPicture.h"

#include <cmath>

namespace Platform
{

PixelRectangle FitPicture(WindowSize windowSize)
{
    float windowWidth = static_cast<float>(windowSize.width);
    float windowHeight = static_cast<float>(windowSize.height);

    float pictureWidth = windowWidth;
    float pictureHeight = std::round(windowWidth * PictureHeightShare / PictureWidthShare);

    bool windowIsTooWide = pictureHeight > windowHeight;
    if (windowIsTooWide)
    {
        pictureHeight = windowHeight;
        pictureWidth = std::round(windowHeight * PictureWidthShare / PictureHeightShare);
    }

    PixelRectangle pictureArea;
    pictureArea.left = (windowWidth - pictureWidth) / 2;
    pictureArea.top = (windowHeight - pictureHeight) / 2;
    pictureArea.width = pictureWidth;
    pictureArea.height = pictureHeight;
    return pictureArea;
}

}
