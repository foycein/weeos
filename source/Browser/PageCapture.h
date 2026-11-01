#pragma once

#include "Formats/RgbaImage.h"

#include <winrt/Windows.UI.Composition.h>

#include <functional>
#include <memory>

namespace Browser
{

// Windows' screen capture (Windows.Graphics.Capture) of one visual of Windows' composition, which no window shows. Windows hands over a
// new picture whenever the content of the visual changes, on a thread of its own; the capture copies it from the graphics card and gives
// it on as an RGBA picture.
class PageCapture
{
public:
    using PictureReceiver = std::function<void(Formats::RgbaImage&& picture)>;

    // Starts at once. The receiver is called on a thread of Windows, one picture at a time. Throws a BrowserError when Windows cannot
    // capture the visual.
    PageCapture(const winrt::Windows::UI::Composition::Visual& visual, PictureReceiver receiver);
    ~PageCapture();

    PageCapture(const PageCapture&) = delete;
    PageCapture& operator=(const PageCapture&) = delete;

private:
    struct Parts;
    std::unique_ptr<Parts> parts;
};

}
