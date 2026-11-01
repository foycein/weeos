#pragma once

#include "Mii/MiiExpression.h"
#include "Mii/MiiHeadModel.h"
#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

#include <cstdint>

namespace Mii
{

// The light of the face library's icons: white, in front of the face above and to the right, with a gray ambient light.
// It stays with the head: the matrix brings the head's units into view space.
Graphics::PointLight MakeHeadLight(const Graphics::Matrix4& headModelView);

// A Mii's head ready to draw: its 3D parts (meshes that stay on the card) and its textures on the graphics device, the face texture
// showing one expression.
class MiiHead
{
public:
    // Builds the parts and draws the face texture, so it must not be made while an offscreen picture is being drawn.
    MiiHead(Graphics::GraphicsDevice& graphicsDevice, const MiiResources& resources, const MiiRecord& miiRecord, MiiExpression expression,
        std::uint32_t faceTextureSize);
    ~MiiHead();

    MiiHead(const MiiHead&) = delete;
    MiiHead& operator=(const MiiHead&) = delete;

    // Draws the opaque parts and then the translucent ones the way the face library does (docs/research/Mii.md section 8).
    // The placement gives the matrices and the light; the head sets the culling and the depth of each part itself.
    void Draw(const Graphics::MeshPlacement& placement) const;

private:
    // The parts of the head on the graphics card. An empty part (no beard, a bald head) has no mesh.
    struct StoredParts
    {
        Graphics::MeshHandle faceline;
        Graphics::MeshHandle cap;
        Graphics::MeshHandle hair;
        Graphics::MeshHandle forehead;
        Graphics::MeshHandle beard;
        Graphics::MeshHandle nose;
        Graphics::MeshHandle noseline;
        Graphics::MeshHandle mask;
        Graphics::MeshHandle glasses;
    };

    void StoreParts(const MiiHeadModel& model);
    Graphics::MeshHandle StorePart(const std::vector<Graphics::MeshVertex>& corners);
    void DrawPart(Graphics::MeshHandle part, const Graphics::MeshPlacement& placement, Graphics::FaceCulling culling,
        Graphics::DepthMode depth, Graphics::Material material) const;

    Graphics::GraphicsDevice& device;
    MiiRecord record;
    StoredParts parts;
    bool hairIsMirrored = false;
    Graphics::TextureHandle faceTexture;
    Graphics::TextureHandle makeUpTexture;
    Graphics::TextureHandle capTexture;
    Graphics::TextureHandle noselineTexture;
    Graphics::TextureHandle glassesTexture;
};

}
