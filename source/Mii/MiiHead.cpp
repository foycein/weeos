#include "Mii/MiiHead.h"

#include "Mii/MiiColors.h"
#include "Mii/MiiFaceTexture.h"
#include "Mii/MiiTextureUpload.h"

namespace Mii
{

namespace
{

using Graphics::DepthMode;
using Graphics::FaceCulling;
using Graphics::TevAlphaInput;
using Graphics::TevColorInput;

constexpr Formats::Vector3 HeadLightPosition = {1600.0f, 1500.0f, 6000.0f};
constexpr Formats::Rgba8Color HeadLightColor = {255, 255, 255, 255};
constexpr Formats::Rgba8Color HeadAmbientColor = {160, 160, 160, 255};

Graphics::RegisterColor ToRegister(Formats::Rgba8Color color)
{
    return {color.red, color.green, color.blue, color.alpha};
}

// Every part is laid over what is there, and its fully transparent pixels are not drawn (the alpha test of the face library).
Graphics::Material MakePartMaterial(Graphics::TevStage stage)
{
    Graphics::Material material;
    material.blendMode = Graphics::BlendMode::Over;
    material.skipsTransparentPixels = true;
    material.stages = {stage};
    return material;
}

Graphics::TevStage MakeStage()
{
    Graphics::TevStage stage;
    stage.usesRasterColor = false;
    return stage;
}

// Beard, nose, forehead and hair: one opaque color.
Graphics::Material MakeColorMaterial(Formats::Rgba8Color color)
{
    Graphics::TevStage stage = MakeStage();
    stage.color.inputD = TevColorInput::Register0Color;
    stage.alpha.inputD = TevAlphaInput::Register0Alpha;

    Graphics::Material material = MakePartMaterial(stage);
    material.colorRegisters[0] = ToRegister(color);
    return material;
}

// The cap takes the favorite color, shaded by its picture: color = favorite x (1 + picture) / 2.
Graphics::Material MakeCapMaterial(Formats::Rgba8Color favoriteColor, Graphics::TextureHandle texture)
{
    Graphics::TevStage stage = MakeStage();
    stage.textureIndex = 0;
    stage.color.inputB = TevColorInput::Register0Color;
    stage.color.inputC = TevColorInput::TextureColor;
    stage.color.inputD = TevColorInput::Register0Color;
    stage.color.scale = Graphics::TevScale::Half;
    stage.alpha.inputD = TevAlphaInput::Register0Alpha;

    Graphics::Material material = MakePartMaterial(stage);
    material.textures = {{texture, {}}};
    material.colorRegisters[0] = ToRegister(favoriteColor);
    return material;
}

// The skin with the make-up or wrinkles over it: the picture's alpha (its green before the swap) is how much of its color covers
// the skin.
Graphics::Material MakeFacelineMaterial(Formats::Rgba8Color skinColor, Graphics::TextureHandle makeUpTexture)
{
    Graphics::TevStage stage = MakeStage();
    stage.textureIndex = 0;
    stage.color.inputA = TevColorInput::Register0Color;
    stage.color.inputB = TevColorInput::TextureColor;
    stage.color.inputC = TevColorInput::TextureAlpha;
    stage.alpha.inputD = TevAlphaInput::Register0Alpha;

    Graphics::Material material = MakePartMaterial(stage);
    material.textures = {{makeUpTexture, {}}};
    material.colorRegisters[0] = ToRegister(skinColor);
    return material;
}

Graphics::Material MakeMaskMaterial(Graphics::TextureHandle faceTexture)
{
    Graphics::TevStage stage = MakeStage();
    stage.textureIndex = 0;
    stage.color.inputD = TevColorInput::TextureColor;
    stage.alpha.inputD = TevAlphaInput::TextureAlpha;

    Graphics::Material material = MakePartMaterial(stage);
    material.textures = {{faceTexture, {}}};
    return material;
}

// The line of the nose is black where its picture is.
Graphics::Material MakeNoselineMaterial(Graphics::TextureHandle noselineTexture)
{
    Graphics::TevStage stage = MakeStage();
    stage.textureIndex = 0;
    stage.alpha.inputD = TevAlphaInput::TextureAlpha;

    Graphics::Material material = MakePartMaterial(stage);
    material.textures = {{noselineTexture, {}}};
    return material;
}

Graphics::Material MakeGlassesMaterial(Formats::Rgba8Color glassesColor, Graphics::TextureHandle glassesTexture)
{
    Graphics::TevStage stage = MakeStage();
    stage.textureIndex = 0;
    stage.color.inputB = TevColorInput::Register0Color;
    stage.color.inputC = TevColorInput::TextureColor;
    stage.alpha.inputD = TevAlphaInput::TextureAlpha;

    Graphics::Material material = MakePartMaterial(stage);
    material.textures = {{glassesTexture, {}}};
    material.colorRegisters[0] = ToRegister(glassesColor);
    return material;
}

// With a light, a second stage multiplies the color by the light at the corners.
void AddLightStage(Graphics::Material& material)
{
    Graphics::TevStage stage;
    stage.usesRasterColor = true;
    stage.color.inputB = TevColorInput::PreviousColor;
    stage.color.inputC = TevColorInput::RasterColor;
    stage.alpha.inputD = TevAlphaInput::PreviousAlpha;
    material.stages.push_back(stage);
}

Graphics::TextureHandle UploadPartTexture(Graphics::GraphicsDevice& device, const MiiResources& resources, TexturePart part, int entry)
{
    MiiTexture texture = resources.ReadTexture(part, entry);
    return UploadTexture(device, texture.image, texture);
}

}

Graphics::PointLight MakeHeadLight(const Graphics::Matrix4& headModelView)
{
    return {Graphics::TransformPoint(headModelView, HeadLightPosition), HeadLightColor, HeadAmbientColor};
}

MiiHead::MiiHead(Graphics::GraphicsDevice& graphicsDevice, const MiiResources& resources, const MiiRecord& miiRecord,
    MiiExpression expression, std::uint32_t faceTextureSize)
    : device(graphicsDevice)
    , record(miiRecord)
{
    MiiHeadModel model = BuildMiiHead(resources, miiRecord);
    StoreParts(model);
    hairIsMirrored = model.hairIsMirrored;

    faceTexture = DrawMiiFaceTexture(device, resources, ApplyExpression(record, expression), faceTextureSize);

    MiiTexture makeUp = resources.ReadTexture(TexturePart::FacelineMakeUp, record.face.facialFeature);
    makeUpTexture = UploadTexture(device, SwapGreenAndAlpha(makeUp.image), makeUp);

    if (parts.cap.IsValid())
        capTexture = UploadPartTexture(device, resources, TexturePart::Cap, record.hair.type);

    noselineTexture = UploadPartTexture(device, resources, TexturePart::Noseline, record.nose.type);
    glassesTexture = UploadPartTexture(device, resources, TexturePart::Glasses, record.glasses.type);
}

MiiHead::~MiiHead()
{
    device.DestroyTexture(faceTexture);
    device.DestroyTexture(makeUpTexture);
    if (capTexture.IsValid())
        device.DestroyTexture(capTexture);
    device.DestroyTexture(noselineTexture);
    device.DestroyTexture(glassesTexture);

    for (Graphics::MeshHandle part : {parts.faceline, parts.cap, parts.hair, parts.forehead, parts.beard, parts.nose, parts.noseline,
             parts.mask, parts.glasses})
    {
        if (part.IsValid())
            device.DestroyMesh(part);
    }
}

void MiiHead::Draw(const Graphics::MeshPlacement& placement) const
{
    FaceCulling hairCulling = FaceCulling::Back;
    if (hairIsMirrored)
        hairCulling = FaceCulling::Front;

    Formats::Rgba8Color skinColor = SkinColors[record.face.skinColor];

    DrawPart(parts.beard, placement, FaceCulling::Back, DepthMode::TestAndWrite, MakeColorMaterial(HairColors[record.facialHair.color]));
    DrawPart(parts.nose, placement, FaceCulling::Back, DepthMode::TestAndWrite, MakeColorMaterial(skinColor));
    DrawPart(parts.forehead, placement, hairCulling, DepthMode::TestAndWrite, MakeColorMaterial(skinColor));
    DrawPart(parts.hair, placement, hairCulling, DepthMode::TestAndWrite, MakeColorMaterial(HairColors[record.hair.color]));
    if (capTexture.IsValid())
    {
        Graphics::Material capMaterial = MakeCapMaterial(FavoriteColors[record.favoriteColor], capTexture);
        DrawPart(parts.cap, placement, hairCulling, DepthMode::TestAndWrite, capMaterial);
    }
    DrawPart(parts.faceline, placement, FaceCulling::Back, DepthMode::TestAndWrite, MakeFacelineMaterial(skinColor, makeUpTexture));

    DrawPart(parts.mask, placement, FaceCulling::Back, DepthMode::TestOnly, MakeMaskMaterial(faceTexture));
    DrawPart(parts.noseline, placement, FaceCulling::Back, DepthMode::TestOnly, MakeNoselineMaterial(noselineTexture));
    DrawPart(parts.glasses, placement, FaceCulling::None, DepthMode::TestOnly,
        MakeGlassesMaterial(GlassesColors[record.glasses.color], glassesTexture));
}

void MiiHead::StoreParts(const MiiHeadModel& model)
{
    parts.faceline = StorePart(model.faceline);
    parts.cap = StorePart(model.cap);
    parts.hair = StorePart(model.hair);
    parts.forehead = StorePart(model.forehead);
    parts.beard = StorePart(model.beard);
    parts.nose = StorePart(model.nose);
    parts.noseline = StorePart(model.noseline);
    parts.mask = StorePart(model.mask);
    parts.glasses = StorePart(model.glasses);
}

Graphics::MeshHandle MiiHead::StorePart(const std::vector<Graphics::MeshVertex>& corners)
{
    if (corners.empty())
        return {};

    return device.CreateMesh(corners);
}

void MiiHead::DrawPart(Graphics::MeshHandle part, const Graphics::MeshPlacement& placement, Graphics::FaceCulling culling,
    Graphics::DepthMode depth, Graphics::Material material) const
{
    if (!part.IsValid())
        return;

    Graphics::MeshPlacement partPlacement = placement;
    partPlacement.culling = culling;
    partPlacement.depth = depth;

    if (placement.light)
        AddLightStage(material);

    device.DrawMesh(part, partPlacement, material);
}

}
