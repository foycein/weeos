#pragma once

#include "Formats/RgbaImage.h"

#include <windows.h>

#include <functional>
#include <memory>

namespace Browser
{

// Windows' screen capture (Windows.Graphics.Capture) of one window. Windows hands over a new picture whenever the window's content
// changes, on a thread of its own; the capture copies it from the graphics card and gives it on as an RGBA picture.
class PageCapture
{
public:
    using PictureReceiver = std::function<void(Formats::RgbaImage&& picture)>;

    // Starts at once. The receiver is called on a thread of Windows, one picture at a time. Throws a BrowserError when Windows cannot
    // capture the window.
    PageCapture(HWND window, PictureReceiver receiver);
    ~PageCapture();

    PageCapture(const PageCapture&) = delete;
    PageCapture& operator=(const PageCapture&) = delete;

private:
    struct Parts;
    std::unique_ptr<Parts> parts;
};

}
