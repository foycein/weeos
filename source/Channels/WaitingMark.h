#pragma once

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"

#include <cstddef>
#include <string_view>

namespace Channels
{

// How a channel draws its waiting mark: six copies of one picture of a TPL file in a row under "One moment, please...", lighting up one
// after the other while the others fade (the Forecast's 0x8000B6B0, the News' 0x80009710).
struct WaitingMarkLook
{
    // The packed TPL file in the channel's resource content and the picture in it.
    std::string_view texturePath;
    std::size_t textureIndex;

    // In pixels of the screen from its top left corner: the distance from the middle of one copy to the next (the row stands in the
    // middle of the screen) and the line the middles stand on.
    float distance;
    float middleY;

    // The copies are drawn at this part of the picture's size.
    float scale;

    // The step counter goes from 0 up to this number and starts again; every copy lights for 12 steps.
    int lastStep;

    // The News: the copies that went out fade along a quarter of a sine instead of a straight line, and a lit copy grows by up to
    // 8 pixels and rises with it.
    bool fadesAlongSine;
    bool growsWhenLit;
};

// The waiting mark of one channel, its picture on the graphics device.
class WaitingMark
{
public:
    // Throws a ChannelError or a FormatError when the picture is not there.
    WaitingMark(Graphics::GraphicsDevice& graphicsDevice, const Formats::U8Archive& resources, const WaitingMarkLook& markLook);
    ~WaitingMark();

    WaitingMark(const WaitingMark&) = delete;
    WaitingMark& operator=(const WaitingMark&) = delete;

    // One step of the mark.
    void Update();

    // Draws the six copies on a picture of the given width (832 for 16:9), whose middle is the origin of the canvas.
    void Draw(float screenWidth);

    // How strong copy number copy (0 to 5) shines now, from 0 (gone) to 255 (lit).
    int AlphaOf(int copy) const;

    int Step() const;

private:
    Graphics::GraphicsDevice& device;
    WaitingMarkLook look;
    Graphics::TextureHandle texture;
    float width = 0;
    float height = 0;
    int step = 0;
};

}
