#pragma once

#include "Formats/BoardMessageFile.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/LiveLayout.h"

#include <memory>

namespace SystemMenu
{

// The picture of a letter: the JPEG file of its picture attachment, decoded by the Media part and put on the graphics device, as the
// board decodes a picture of the kind 1 (0x8135B5A0). The texture lives as long as this object.
class LetterPicture
{
public:
    // Nothing when the letter has no picture or its picture cannot be decoded; the letter is then shown without it.
    static std::unique_ptr<LetterPicture> Of(Graphics::GraphicsDevice& device, const Formats::BoardMessage& message);

    LetterPicture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image);
    ~LetterPicture();

    LetterPicture(const LetterPicture&) = delete;
    LetterPicture& operator=(const LetterPicture&) = delete;

    const Layout::LoadedTexture& Texture() const;

    // Width divided by height.
    float Aspect() const;

private:
    Graphics::GraphicsDevice& device;
    Layout::LoadedTexture texture;
};

}
