#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Graphics/GraphicsDevice.h"

#include <vector>

namespace Channels
{

// The ground of the Mii Plaza, the model graund_model of graund_model.brres in Character.szs: 114 triangles from -4700 to 4700 across and
// from 0 to 5400 in depth, the paving picture ground (256 x 256, eight bits into a palette of 48 colors) repeated six times, and corner
// colors whose alpha fades the paving into white toward the edges.
struct PlazaGroundModel
{
    std::vector<Graphics::MeshVertex> triangleCorners;
    Formats::RgbaImage paving;
};

// Throws a FormatError when the file is not the ground the plaza draws.
PlazaGroundModel ReadPlazaGroundModel(Formats::ByteView brres);

// The ground on the graphics card, drawn as its material mt_ground says: color = paving x (1 - corner alpha) + white x corner alpha,
// the paving read six times across in both directions and mirrored at every edge.
class PlazaGround
{
public:
    PlazaGround(Graphics::GraphicsDevice& graphicsDevice, const PlazaGroundModel& model);
    ~PlazaGround();

    PlazaGround(const PlazaGround&) = delete;
    PlazaGround& operator=(const PlazaGround&) = delete;

    void Draw(const Graphics::Matrix4& modelView, const Graphics::Matrix4& projection) const;

private:
    Graphics::GraphicsDevice& device;
    Graphics::MeshHandle mesh;
    Graphics::TextureHandle paving;
};

}
