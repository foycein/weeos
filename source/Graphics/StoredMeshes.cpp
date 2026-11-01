#include "Graphics/StoredMeshes.h"

#include "Graphics/DrawData.h"
#include "Graphics/GraphicsError.h"

#include <format>
#include <optional>

namespace Graphics
{

namespace
{

constexpr VkMemoryPropertyFlags ReachableByTheProgram = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

// The allocator counts corners, and a corner may start anywhere.
constexpr std::uint64_t CornerAlignment = 1;

}

StoredMeshes::StoredMeshes(const VulkanContext& context)
    : cornerBuffer(context, StoredMeshCornerCapacity * sizeof(ShaderMeshCorner), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
    , freeCorners(StoredMeshCornerCapacity)
{
}

MeshHandle StoredMeshes::Create(std::span<const MeshVertex> triangleCorners)
{
    if (triangleCorners.empty())
        throw GraphicsError("A stored mesh needs at least one triangle");

    std::optional<std::uint64_t> firstCorner = freeCorners.Allocate(triangleCorners.size(), CornerAlignment);
    if (!firstCorner)
        throw GraphicsError(std::format("The stored meshes have no room for {} more corners", triangleCorners.size()));

    auto* corners = static_cast<ShaderMeshCorner*>(cornerBuffer.MappedBytes());
    for (std::size_t cornerNumber = 0; cornerNumber < triangleCorners.size(); ++cornerNumber)
        corners[*firstCorner + cornerNumber] = MakeMeshCorner(triangleCorners[cornerNumber]);

    MeshHandle handle = {nextNumber};
    ++nextNumber;
    meshes[handle.number] = {static_cast<std::uint32_t>(*firstCorner), static_cast<std::uint32_t>(triangleCorners.size())};
    return handle;
}

StoredMesh StoredMeshes::Get(MeshHandle mesh) const
{
    auto found = meshes.find(mesh.number);
    if (found == meshes.end())
        throw GraphicsError(std::format("There is no stored mesh {}", mesh.number));

    return found->second;
}

void StoredMeshes::Retire(MeshHandle mesh, std::uint64_t lastFrameUsing)
{
    retiredMeshes.push_back({Get(mesh), lastFrameUsing});
    meshes.erase(mesh.number);
}

void StoredMeshes::ReleaseFinished(std::uint64_t finishedFrameNumber)
{
    std::vector<RetiredMesh> stillInUse;
    for (const RetiredMesh& retired : retiredMeshes)
    {
        if (retired.lastFrameUsing > finishedFrameNumber)
            stillInUse.push_back(retired);
        else
            freeCorners.Free(retired.mesh.firstCorner, retired.mesh.cornerCount);
    }
    retiredMeshes = stillInUse;
}

VkBuffer StoredMeshes::Buffer() const
{
    return cornerBuffer.Handle();
}

}
