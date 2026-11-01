#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/VulkanContext.h"

#include <array>

namespace Graphics
{

// The descriptor sets of the pipelines: set 0 holds the buffers with the draw data, and sets 1 to 3 hold the textures 0 to 2.
constexpr std::uint32_t DrawDataSetNumber = 0;
constexpr std::uint32_t FirstTextureSetNumber = 1;
constexpr std::uint32_t DescriptorSetCount = FirstTextureSetNumber + static_cast<std::uint32_t>(MaximumTextureCount);

// The bindings of set 0: draw records, stages, meshes, the corners of the meshes, the corners of the stored meshes and the bones.
constexpr std::uint32_t DrawDataBindingCount = 6;

// Pushed to the vertex shaders before the draws. The picture is the whole target, which can be larger than the canvas in its middle:
// half its width and height in canvas units, and the share of the picture's width and height that the canvas covers.
struct CanvasConstants
{
    float halfPictureWidth;
    float halfPictureHeight;
    float canvasShareOfWidth;
    float canvasShareOfHeight;
};

// The shaders and the pipelines that draw quads and meshes. Both run the stages of the material in the same fragment program;
// the quad vertex program makes the corners of a quad from the draw data, the mesh vertex program transforms and lights the
// corners of a mesh. There is a quad pipeline and a mesh pipeline for every blend mode. The pictures have a depth buffer: the
// quads ignore it, the meshes test and write it as their placement says (dynamic state, set before each mesh).
// The shaders are SPIR-V files that sit beside the program.
class DrawPipelines
{
public:
    explicit DrawPipelines(const VulkanContext& graphicsContext);
    ~DrawPipelines();

    DrawPipelines(const DrawPipelines&) = delete;
    DrawPipelines& operator=(const DrawPipelines&) = delete;

    VkPipeline QuadPipelineFor(BlendMode blendMode) const;
    VkPipeline MeshPipelineFor(BlendMode blendMode) const;
    VkPipelineLayout Layout() const;
    VkDescriptorSetLayout DrawDataSetLayout() const;
    VkDescriptorSetLayout TextureSetLayout() const;

private:
    enum class Shape
    {
        Quad,
        Mesh,
    };

    VkPipeline MakePipeline(Shape shape, BlendMode blendMode, VkShaderModule vertexShader, VkShaderModule fragmentShader) const;

    const VulkanContext& context;
    VkDescriptorSetLayout drawDataSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::array<VkPipeline, BlendModeCount> quadPipelines = {};
    std::array<VkPipeline, BlendModeCount> meshPipelines = {};
};

}
