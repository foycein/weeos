#pragma once

#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/RangeAllocator.h"
#include "Graphics/VulkanContext.h"

#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace Graphics
{

// How many corners all the meshes that stay on the graphics card can have together: a hundred Miis of the Mii Plaza with
// their heads, with room to spare.
constexpr std::size_t StoredMeshCornerCapacity = 1048576;

// Where the corners of one stored mesh are in the buffer.
struct StoredMesh
{
    std::uint32_t firstCorner = 0;
    std::uint32_t cornerCount = 0;
};

// The meshes that stay on the graphics card (GraphicsDevice::CreateMesh): their corners in one buffer that the vertex shader
// reads, so that a mesh is copied once and not every time it is drawn.
class StoredMeshes
{
public:
    explicit StoredMeshes(const VulkanContext& context);

    StoredMeshes(const StoredMeshes&) = delete;
    StoredMeshes& operator=(const StoredMeshes&) = delete;

    // Throws a GraphicsError when the buffer has no room left for the corners.
    MeshHandle Create(std::span<const MeshVertex> triangleCorners);

    // Throws a GraphicsError for a handle that names no mesh.
    StoredMesh Get(MeshHandle mesh) const;

    // The corners stay until the picture with the number lastFrameUsing is finished.
    void Retire(MeshHandle mesh, std::uint64_t lastFrameUsing);
    void ReleaseFinished(std::uint64_t finishedFrameNumber);

    VkBuffer Buffer() const;

private:
    struct RetiredMesh
    {
        StoredMesh mesh;
        std::uint64_t lastFrameUsing;
    };

    GraphicsBuffer cornerBuffer;
    RangeAllocator freeCorners;
    std::map<std::uint32_t, StoredMesh> meshes;
    std::vector<RetiredMesh> retiredMeshes;
    std::uint32_t nextNumber = 1;
};

}
