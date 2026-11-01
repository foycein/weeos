#include "Mii/MiiFaceTexture.h"

#include "Mii/MiiColors.h"
#include "Mii/MiiFaceLayout.h"
#include "Mii/MiiTextureUpload.h"

#include <vector>

namespace Mii
{

namespace
{

using Graphics::TevAlphaInput;
using Graphics::TevColorInput;

// Assumed (docs/audit/OpenPoints/Mii.md MII-16): where the Wii samples a pixel of this picture is not documented; the quads are
// placed as they are and Vulkan samples at the middle of the pixel.
constexpr float PixelCenterOffset = 0.0f;

constexpr Formats::Rgba8Color White = {255, 255, 255, 255};

// The device textures of the pictures one face needs. The eyes and the mouth have one texture per color channel.
struct FacePictures
{
    std::vector<Graphics::TextureHandle> eye;
    std::vector<Graphics::TextureHandle> mouth;
    Graphics::TextureHandle eyebrow;
    Graphics::TextureHandle mustache;
    Graphics::TextureHandle mole;
};

std::vector<Graphics::TextureHandle> UploadChannels(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image,
    const std::vector<ColorChannel>& channels)
{
    std::vector<Graphics::TextureHandle> textures;
    for (ColorChannel channel : channels)
        textures.push_back(UploadClampedTexture(device, SpreadChannel(image, channel)));
    return textures;
}

FacePictures UploadFacePictures(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiRecord& record)
{
    Formats::RgbaImage eye = resources.ReadTexture(TexturePart::Eye, record.eyes.type).image;
    Formats::RgbaImage mouth = resources.ReadTexture(TexturePart::Mouth, record.mouth.type).image;
    int moleEntry = record.mole.isVisible ? 1 : 0;

    FacePictures pictures;
    pictures.eye = UploadChannels(device, eye, {ColorChannel::Red, ColorChannel::Blue, ColorChannel::Green});
    pictures.mouth = UploadChannels(device, mouth, {ColorChannel::Red, ColorChannel::Green, ColorChannel::Blue});
    pictures.eyebrow = UploadClampedTexture(device, resources.ReadTexture(TexturePart::Eyebrow, record.eyebrows.type).image);
    pictures.mustache = UploadClampedTexture(device, resources.ReadTexture(TexturePart::Mustache, record.facialHair.mustacheType).image);
    pictures.mole = UploadClampedTexture(device, resources.ReadTexture(TexturePart::Mole, moleEntry).image);
    return pictures;
}

void DestroyFacePictures(Graphics::GraphicsDevice& device, const FacePictures& pictures)
{
    for (Graphics::TextureHandle texture : pictures.eye)
        device.DestroyTexture(texture);
    for (Graphics::TextureHandle texture : pictures.mouth)
        device.DestroyTexture(texture);
    device.DestroyTexture(pictures.eyebrow);
    device.DestroyTexture(pictures.mustache);
    device.DestroyTexture(pictures.mole);
}

Graphics::RegisterColor ToRegister(Formats::Rgba8Color color)
{
    return {color.red, color.green, color.blue, color.alpha};
}

// Every quad goes behind what was drawn before, and its fully transparent pixels are not drawn at all.
Graphics::Material MakeFaceMaterial()
{
    Graphics::Material material;
    material.blendMode = Graphics::BlendMode::Behind;
    material.skipsTransparentPixels = true;
    return material;
}

// One color with the alpha of the picture: eyebrows, mustache and mole.
Graphics::Material MakeFlatMaterial(Graphics::TextureHandle texture, Formats::Rgba8Color color)
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = false;
    stage.color.inputD = TevColorInput::Register0Color;
    stage.alpha.inputD = TevAlphaInput::TextureAlpha;

    Graphics::Material material = MakeFaceMaterial();
    material.textures = {{texture, {}}};
    material.colorRegisters[0] = ToRegister(color);
    material.stages = {stage};
    return material;
}

// Three channels, each colored by its own tint and added up: color = first x tint 0 + second x tint 1 + third x tint 2.
// The alpha is the alpha of the picture.
Graphics::Material MakeThreeChannelMaterial(const std::vector<Graphics::TextureHandle>& channelTextures,
    Formats::Rgba8Color firstTint, Formats::Rgba8Color secondTint)
{
    std::vector<TevColorInput> tints = {TevColorInput::Register0Color, TevColorInput::Register1Color, TevColorInput::One};

    Graphics::Material material = MakeFaceMaterial();
    material.colorRegisters[0] = ToRegister(firstTint);
    material.colorRegisters[1] = ToRegister(secondTint);

    for (std::uint8_t stageNumber = 0; stageNumber < tints.size(); ++stageNumber)
    {
        bool isFirst = stageNumber == 0;

        Graphics::TevStage stage;
        stage.textureIndex = stageNumber;
        stage.usesRasterColor = false;
        stage.color.inputB = tints[stageNumber];
        stage.color.inputC = TevColorInput::TextureColor;
        stage.color.inputD = isFirst ? TevColorInput::Zero : TevColorInput::PreviousColor;
        stage.alpha.inputD = isFirst ? TevAlphaInput::TextureAlpha : TevAlphaInput::PreviousAlpha;

        material.textures.push_back({channelTextures[stageNumber], {}});
        material.stages.push_back(stage);
    }
    return material;
}

Graphics::Material MaterialFor(FaceFeature feature, const FacePictures& pictures, const MiiRecord& record)
{
    switch (feature)
    {
    case FaceFeature::Mustache:
        return MakeFlatMaterial(pictures.mustache, HairColors[record.facialHair.color]);
    case FaceFeature::Mouth:
        return MakeThreeChannelMaterial(pictures.mouth, LipColors[record.mouth.color], UpperLipColors[record.mouth.color]);
    case FaceFeature::Eyebrow:
        return MakeFlatMaterial(pictures.eyebrow, HairColors[record.eyebrows.color]);
    case FaceFeature::Eye:
        return MakeThreeChannelMaterial(pictures.eye, EyeOutlineColor(record.eyes.type), EyeColors[record.eyes.color]);
    case FaceFeature::Mole:
        return MakeFlatMaterial(pictures.mole, MoleColor);
    }
    return MakeFaceMaterial();
}

// The layout has y downward from the top left corner; the canvas has y upward from the middle.
Graphics::Quad ToCanvasQuad(const FaceFeatureQuad& featureQuad, float size)
{
    float half = size / 2;

    Graphics::Quad quad = {};
    for (std::size_t corner = 0; corner < Graphics::QuadCornerCount; ++corner)
    {
        const Formats::Vector2& position = featureQuad.corners[corner];
        quad.positions[corner] = {position.x - half + PixelCenterOffset, half - position.y - PixelCenterOffset};
        quad.colors[corner] = White;
        for (std::size_t textureNumber = 0; textureNumber < Graphics::MaximumTextureCount; ++textureNumber)
            quad.textureCoordinates[textureNumber][corner] = featureQuad.textureCoordinates[corner];
    }
    return quad;
}

}

Graphics::TextureHandle DrawMiiFaceTexture(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiRecord& record,
    std::uint32_t size)
{
    FacePictures pictures = UploadFacePictures(device, resources, record);
    auto canvasSize = static_cast<float>(size);

    Graphics::TextureHandle faceTexture = device.BeginOffscreen({canvasSize, canvasSize}, size, size);
    for (const FaceFeatureQuad& featureQuad : LayOutFace(record, canvasSize))
        device.DrawQuad(ToCanvasQuad(featureQuad, canvasSize), MaterialFor(featureQuad.feature, pictures, record));
    device.EndOffscreen();

    DestroyFacePictures(device, pictures);
    return faceTexture;
}

}
