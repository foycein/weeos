#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/Bytes.h"
#include "Formats/EffectFile.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Effects
{

// The effect files a program has loaded (.breff with its .breft) and their pictures on the graphics device.
class EffectResources
{
public:
    // The device must outlive the resources.
    explicit EffectResources(Graphics::GraphicsDevice& graphicsDevice);
    ~EffectResources();
    EffectResources(const EffectResources&) = delete;
    EffectResources& operator=(const EffectResources&) = delete;

    void AddFiles(Formats::ByteView effectFile, Formats::ByteView textureFile);

    // Searches the files from the last one added, as the library does. Null when no file has the name.
    const Formats::EmitterResource* FindEmitter(std::string_view name) const;

    // The picture with this name on the device, with the wrap of a particle (four bits: horizontal in bits 0 and 1, vertical in bits
    // 2 and 3). An invalid handle when no file has the picture.
    Graphics::TextureHandle Texture(std::string_view name, std::uint8_t wrapBits);

    Graphics::GraphicsDevice& Device();

private:
    Graphics::GraphicsDevice& device;
    std::vector<Formats::EffectFile> files;
    std::map<std::string, Formats::EffectTexture, std::less<>> pictures;
    std::map<std::pair<std::string, std::uint8_t>, Graphics::TextureHandle> uploadedPictures;
};

}
