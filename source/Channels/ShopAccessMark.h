#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/Bytes.h"

#include <array>

namespace Channels
{

// Where the Shop shows its access mark (0x8001BC60): over the start-up picture until the first page is drawn, and small in the upper left
// corner over a page while it loads again.
enum class AccessMarkPlace
{
    OverTheStartUpPicture,
    OverThePage,
};

// The Wii Shop Channel's access mark: the model of AccessMark.brres, a turning mark with its shadow behind it, one turn a second.
// The model is one square of 4 by 4 units for the mark and one for the shadow; it is drawn as two quads with the pictures of the file.
class ShopAccessMark
{
public:
    // The device must outlive the mark.
    ShopAccessMark(Graphics::GraphicsDevice& graphicsDevice, Formats::ByteView accessMarkFile);
    ~ShopAccessMark();
    ShopAccessMark(const ShopAccessMark&) = delete;
    ShopAccessMark& operator=(const ShopAccessMark&) = delete;

    // One step of the turn, while the mark shows.
    void Update();

    void Draw(AccessMarkPlace place) const;

    // The turn of the mark now, in degrees, clockwise.
    float TurnDegrees() const;

private:
    // The pictures of the file: .1 over the start-up picture, .2 over a page (the frames of its texture pattern).
    std::array<Graphics::TextureHandle, 2> markPictures;
    std::array<Graphics::TextureHandle, 2> shadowPictures;

    Graphics::GraphicsDevice& device;
    float frame = 0;
};

}
