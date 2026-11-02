#include "Effects/EffectResources.h"

namespace Effects
{

namespace
{

constexpr std::uint8_t WrapMask = 0x3;
constexpr int VerticalWrapShift = 2;
constexpr std::uint8_t RepeatNumber = 1;
constexpr std::uint8_t MirrorNumber = 2;

Graphics::WrapMode WrapModeOf(std::uint8_t number)
{
    if (number == RepeatNumber)
        return Graphics::WrapMode::Repeat;
    if (number == MirrorNumber)
        return Graphics::WrapMode::Mirror;
    return Graphics::WrapMode::Clamp;
}

}

EffectResources::EffectResources(Graphics::GraphicsDevice& graphicsDevice)
    : device(graphicsDevice)
{
}

EffectResources::~EffectResources()
{
    for (const auto& [key, uploaded] : uploadedPictures)
        device.DestroyTexture(uploaded.handle);
}

void EffectResources::AddFiles(Formats::ByteView effectFile, Formats::ByteView textureFile)
{
    files.push_back(Formats::ReadEffectFile(effectFile));
    for (Formats::EffectTexture& picture : Formats::ReadEffectTextureFile(textureFile))
    {
        std::string name = picture.name;
        sharpPictures[name] = SharpArt::FindSharpPicture(name, picture.image);
        pictures.insert_or_assign(name, std::move(picture));
    }
}

const Formats::EmitterResource* EffectResources::FindEmitter(std::string_view name) const
{
    for (auto file = files.rbegin(); file != files.rend(); ++file)
    {
        const Formats::EmitterResource* emitter = file->FindEmitter(name);
        if (emitter != nullptr)
            return emitter;
    }
    return nullptr;
}

Graphics::TextureHandle EffectResources::Texture(std::string_view name, std::uint8_t wrapBits)
{
    std::uint32_t sharpPixelsPerUnit = SharpPixelsPerUnitOf(name);
    std::pair<std::string, std::uint8_t> key = {std::string(name), wrapBits};
    auto uploaded = uploadedPictures.find(key);
    if (uploaded != uploadedPictures.end())
    {
        if (uploaded->second.sharpPixelsPerUnit == sharpPixelsPerUnit)
            return uploaded->second.handle;

        device.DestroyTexture(uploaded->second.handle);
        uploadedPictures.erase(uploaded);
    }

    Graphics::TextureHandle texture = Upload(name, sharpPixelsPerUnit, wrapBits);
    if (texture.IsValid())
        uploadedPictures.emplace(key, UploadedPicture{texture, sharpPixelsPerUnit});
    return texture;
}

std::uint32_t EffectResources::SharpPixelsPerUnitOf(std::string_view name) const
{
    auto sharpPicture = sharpPictures.find(name);
    if (sharpPicture == sharpPictures.end() || sharpPicture->second == nullptr)
        return 0;

    return SharpArt::PixelsPerUnitFor(*sharpPicture->second, device.ScreenPixelsPerUnit());
}

Graphics::TextureHandle EffectResources::Upload(std::string_view name, std::uint32_t sharpPixelsPerUnit, std::uint8_t wrapBits)
{
    auto picture = pictures.find(name);
    if (picture == pictures.end())
        return {};

    std::shared_ptr<const Formats::RgbaImage> sharpImage;
    if (sharpPixelsPerUnit > 0)
        sharpImage = SharpArt::KeptSharpPicture(*sharpPictures.find(name)->second, sharpPixelsPerUnit);
    const Formats::RgbaImage& image = sharpImage ? *sharpImage : picture->second.image;

    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;
    description.horizontalWrap = WrapModeOf(wrapBits & WrapMask);
    description.verticalWrap = WrapModeOf((wrapBits >> VerticalWrapShift) & WrapMask);
    description.filter = Graphics::FilterMode::Linear;
    return device.CreateTexture(description);
}

Graphics::GraphicsDevice& EffectResources::Device()
{
    return device;
}

}
