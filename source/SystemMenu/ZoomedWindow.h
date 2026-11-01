#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/TelevisionZoom.h"

#include <functional>

namespace SystemMenu
{

// While a list of channels zooms into a television, the window of the channel is one flat picture inside the television, with black
// around it (0x813B5A54 and 0x813B9BFC for the channel grid, 0x813E5F54 for the SD Card Menu). The window is drawn into a picture of
// the whole screen in the Underlay pass and shown in the Main pass, so it fades as one picture and nothing of it lies outside the
// television.
class ZoomedWindow
{
public:
    // The display must outlive the window.
    explicit ZoomedWindow(Display& display);
    ~ZoomedWindow();

    ZoomedWindow(const ZoomedWindow&) = delete;
    ZoomedWindow& operator=(const ZoomedWindow&) = delete;

    // Underlay pass: what drawWindow draws becomes the picture of the window.
    void DrawPicture(const std::function<void()>& drawWindow);

    // Main pass: the picture lies where the television is now, as opaque as the zoom has come, with black around it.
    void ShowInsideTheTelevision(const TelevisionZoom& zoom);

private:
    void ForgetPicture();

    Display& display;
    Graphics::TextureHandle picture;
};

}
