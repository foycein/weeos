#pragma once

#include "Mii/MiiBodyFile.h"
#include "Mii/MiiRecord.h"

#include "Graphics/GraphicsDevice.h"

#include <cstdint>
#include <vector>

namespace Mii
{

// A body in its standing pose (frame 0 of bodyWait, upright) and in the size of its Mii, as triangle corners. The feet stand on
// y = 0 and the body looks toward +z, like the head.
struct PosedBody
{
    std::vector<Graphics::MeshVertex> shirt;
    std::vector<Graphics::MeshVertex> pants;

    // Where the chin of the head goes (the head bone). The head is drawn there upright and in its own size.
    Formats::Vector3 headPosition;
};

// Poses the body the way the Mii Channel does (docs/research/Mii.md section 10.3): height makes the bones longer, weight
// makes them thicker.
PosedBody PoseStandingBody(const MiiBodyFile& file, const MiiRecord& record);

// The pants of the Mii Channel: gold for a special Mii, gray for a Mii of this console (red for a favorite), blue for others.
Formats::Rgba8Color PantsColorFor(const MiiRecord& record, std::uint32_t consoleSystemId);

// A Mii's body ready to draw.
class MiiBody
{
public:
    MiiBody(Graphics::GraphicsDevice& graphicsDevice, const MiiBodyFiles& files, const MiiRecord& record, std::uint32_t consoleSystemId);
    ~MiiBody();

    MiiBody(const MiiBody&) = delete;
    MiiBody& operator=(const MiiBody&) = delete;

    Formats::Vector3 HeadPosition() const;

    // Draws shirt and pants: their color plus the square of the lit sphere at the direction of the surface. The body has no
    // light of its own, so the light of the placement is not used.
    void Draw(const Graphics::MeshPlacement& placement) const;

private:
    void DrawPart(const std::vector<Graphics::MeshVertex>& corners, const Graphics::MeshPlacement& placement, Formats::Rgba8Color color) const;

    Graphics::GraphicsDevice& device;
    PosedBody body;
    Formats::Rgba8Color shirtColor;
    Formats::Rgba8Color pantsColor;
    Graphics::TextureHandle environmentTexture;
};

}
