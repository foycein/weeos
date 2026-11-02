#pragma once

#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

#include <vector>

namespace Mii
{

// The 3D parts of a head, each as triangle corners in the units of RFL_Res.dat: the chin is at y = 0, the face looks toward +z,
// and the head is about 104 units tall with its hair. An empty part (no beard, a bald head) has no corners.
// Mirrored parts (hair parted on the other side) are turned inside out: draw them with the front side culled.
struct MiiHeadModel
{
    std::vector<Graphics::MeshVertex> faceline;
    std::vector<Graphics::MeshVertex> cap;
    std::vector<Graphics::MeshVertex> hair;
    std::vector<Graphics::MeshVertex> forehead;
    std::vector<Graphics::MeshVertex> beard;
    std::vector<Graphics::MeshVertex> nose;
    std::vector<Graphics::MeshVertex> noseline;
    std::vector<Graphics::MeshVertex> mask;
    std::vector<Graphics::MeshVertex> glasses;
    bool hairIsMirrored = false;
};

// Puts the parts of the record together the way the face library does: hair, cap and
// forehead of the hair type on the hair point of the face, the beard on its beard point, nose and glasses scaled and moved
// from its nose point.
MiiHeadModel BuildMiiHead(const MiiResources& resources, const MiiRecord& record);

}
