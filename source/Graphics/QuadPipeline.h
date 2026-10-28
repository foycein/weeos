#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/VulkanContext.h"

#include <array>

namespace Graphics
{

// The descriptor sets of the pipeline: set 0 holds the buffers with the draw data, and sets 1 to 3 hold the textures 0 to 2.
constexpr std::uint32_t DrawDataSetNumber = 0;
constexpr std::uint32_t FirstTextureSetNumber = 1;

// Pushed to the vertex shader before the draws: half the width and half the height of the canvas.
struct CanvasConstants
{
    float halfWidth;
    float halfHeight;
};

// The shaders and the pipelines that draw quads. The vertex shader makes the corners of a quad from the draw data,
// the fragment shader runs the stages of the material. There is one pipeline for every blend mode.
// The shaders are SPIR-V files that sit beside the program.
class QuadPipeline
{
public:
    explicit QuadPipeline(const VulkanContext& graphicsContext);
    ~QuadPipeline();

    QuadPipeline(const QuadPipeline&) = delete;
    QuadPipeline& operator=(const QuadPipeline&) = delete;

    VkPipeline PipelineFor(BlendMode blendMode) const;
    VkPipelineLayout Layout() const;
    VkDescriptorSetLayout DrawDataSetLayout() const;
    VkDescriptorSetLayout TextureSetLayout() const;

private:
    VkPipeline MakePipeline(BlendMode blendMode, VkShaderModule vertexShader, VkShaderModule fragmentShader) const;

    const VulkanContext& context;
    VkDescriptorSetLayout drawDataSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::array<VkPipeline, BlendModeCount> pipelines = {};
};

}
