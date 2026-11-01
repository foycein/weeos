#include "SystemMenu/MessageBoard/LetterPicture.h"

#include "Formats/FormatError.h"
#include "Media/JpegDecoder.h"

namespace SystemMenu
{

std::unique_ptr<LetterPicture> LetterPicture::Of(Graphics::GraphicsDevice& device, const Formats::BoardMessage& message)
{
    for (const std::optional<Formats::BoardAttachment>& attachment : message.attachments)
    {
        if (!attachment || attachment->kind != Formats::BoardAttachmentKind::Picture)
            continue;

        try
        {
            Formats::RgbaImage image = Media::DecodeJpeg(attachment->contents);
            if (image.width == 0 || image.height == 0)
                return nullptr;
            return std::make_unique<LetterPicture>(device, image);
        }
        catch (const Formats::FormatError&)
        {
            return nullptr;
        }
    }
    return nullptr;
}

LetterPicture::LetterPicture(Graphics::GraphicsDevice& graphicsDevice, const Formats::RgbaImage& image)
    : device(graphicsDevice)
{
    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;
    texture.handle = device.CreateTexture(description);
    texture.width = image.width;
    texture.height = image.height;
}

LetterPicture::~LetterPicture()
{
    device.DestroyTexture(texture.handle);
}

const Layout::LoadedTexture& LetterPicture::Texture() const
{
    return texture;
}

float LetterPicture::Aspect() const
{
    return static_cast<float>(texture.width) / static_cast<float>(texture.height);
}

}
