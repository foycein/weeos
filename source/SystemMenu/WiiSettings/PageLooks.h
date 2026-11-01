#pragma once

#include "SystemMenu/WiiSettings/PageContent.h"

#include <string>

namespace SystemMenu
{

// A picture that is drawn at a place that is counted from the button.
struct FramePicture
{
    std::string imagePath;

    // How far the picture lies from the left and from the top edge of the button. A negative number is outside of the button.
    int offsetFromLeft = 0;
    int offsetFromTop = 0;
};

// The pictures that make a look. The pictures are those of the pages of the original (docs/research/SystemMenuStructure.md Appendix B).
struct LookPictures
{
    // The picture behind the button. It is drawn at its own size in the middle of the button.
    std::string background;

    // The picture over it, stretched to the button: one while the pointer is away and one while it is on the button.
    std::string idleOverlay;
    std::string pointedOverlay;

    // The two parts of the frame that stands around a chosen button. The right part is counted from the right edge of the button.
    FramePicture chosenFrameLeft;
    FramePicture chosenFrameRight;
    bool hasChosenFrame = false;

    // The characters of the text are this high, in points of the page.
    float textSize = 26;
};

const LookPictures& PicturesOfLook(ButtonLook look);

}
