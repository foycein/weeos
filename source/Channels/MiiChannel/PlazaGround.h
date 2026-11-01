#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Graphics/GraphicsDevice.h"

#include <vector>

namespace Channels
{

// Where the corner alpha mixes the picture with white.
enum class WhiteFade
{
    // The paving of the plaza (mt_ground): picture x (1 - alpha) + white x alpha.
    WhereAlphaIsHigh,

    // The road of the Mii Parade (the first stage of mt_road; its second stage, the fade of alpha_tex, is left out, CHN-84):
    // white x (1 - alpha) + picture x alpha.
    WhereAlphaIsLow,
};

// A ground of Character.szs: triangles with corner colors whose alpha fades the picture into white toward the edges.
//  - The plaza's graund_model: 114 triangles from -4700 to 4700 across and from 0 to 5400 in depth, the paving picture ground (256 x 256,
//    eight bits into a palette of 48 colors) repeated six times and mirrored at every edge.
//  - The Mii Parade's road_model: the road from -2700 to 2700 across and from 0 to 5400 in depth, the picture road (512 x 512).
struct PlazaGroundModel
{
    std::vector<Graphics::MeshVertex> triangleCorners;
    Formats::RgbaImage paving;
    Graphics::TextureMatrix pavingMatrix;
    Graphics::WrapMode pavingWrap = Graphics::WrapMode::Repeat;
    WhiteFade fade = WhiteFade::WhereAlphaIsHigh;
};

// Throw a FormatError when the file is not the ground they read.
PlazaGroundModel ReadPlazaGroundModel(Formats::ByteView brres);
PlazaGroundModel ReadParadeRoadModel(Formats::ByteView brres);

// A ground on the graphics card, its picture faded into white by the corner alpha.
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
    Graphics::TextureMatrix pavingMatrix;
    WhiteFade fade;
};

}
