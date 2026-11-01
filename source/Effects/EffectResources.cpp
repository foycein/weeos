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
    for (const auto& [key, texture] : uploadedPictures)
        device.DestroyTexture(texture);
}

void EffectResources::AddFiles(Formats::ByteView effectFile, Formats::ByteView textureFile)
{
    files.push_back(Formats::ReadEffectFile(effectFile));
    for (Formats::EffectTexture& picture : Formats::ReadEffectTextureFile(textureFile))
    {
        std::string name = picture.name;
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
    std::pair<std::string, std::uint8_t> key = {std::string(name), wrapBits};
    auto uploaded = uploadedPictures.find(key);
    if (uploaded != uploadedPictures.end())
        return uploaded->second;

    auto picture = pictures.find(name);
    if (picture == pictures.end())
        return {};

    const Formats::RgbaImage& image = picture->second.image;
    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;
    description.horizontalWrap = WrapModeOf(wrapBits & WrapMask);
    description.verticalWrap = WrapModeOf((wrapBits >> VerticalWrapShift) & WrapMask);
    description.filter = Graphics::FilterMode::Linear;

    Graphics::TextureHandle texture = device.CreateTexture(description);
    uploadedPictures.emplace(key, texture);
    return texture;
}

Graphics::GraphicsDevice& EffectResources::Device()
{
    return device;
}

}
