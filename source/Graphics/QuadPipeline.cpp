#include "Graphics/QuadPipeline.h"

#include "Formats/FileBytes.h"
#include "Graphics/GraphicsError.h"
#include "Graphics/VulkanSupport.h"
#include "Platform/FileLocations.h"

#include <filesystem>

namespace Graphics
{

namespace
{

constexpr const char* ShaderEntryPoint = "main";

VkShaderModule LoadShader(const VulkanContext& context, const char* fileName)
{
    std::filesystem::path path = Platform::ExecutableFolder() / fileName;
    if (!std::filesystem::is_regular_file(path))
        throw GraphicsError("The shader file " + path.string() + " is not there. Building the project makes it.");

    Formats::Bytes code = Formats::ReadFileBytes(path);

    VkShaderModuleCreateInfo shaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(code.data()),
    };
    VkShaderModule shader = VK_NULL_HANDLE;
    CheckVulkan(vkCreateShaderModule(context.Device(), &shaderInfo, nullptr, &shader), "loading a shader");
    return shader;
}

VkDescriptorSetLayout MakeDrawDataSetLayout(const VulkanContext& context)
{
    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding bindings[] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = stages},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = stages},
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings,
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    CheckVulkan(vkCreateDescriptorSetLayout(context.Device(), &layoutInfo, nullptr, &layout), "creating the layout of the draw data");
    return layout;
}

VkDescriptorSetLayout MakeTextureSetLayout(const VulkanContext& context)
{
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    CheckVulkan(vkCreateDescriptorSetLayout(context.Device(), &layoutInfo, nullptr, &layout), "creating the layout of a texture");
    return layout;
}

}

QuadPipeline::QuadPipeline(const VulkanContext& graphicsContext)
    : context(graphicsContext)
{
    drawDataSetLayout = MakeDrawDataSetLayout(context);
    textureSetLayout = MakeTextureSetLayout(context);

    VkDescriptorSetLayout setLayouts[] = {drawDataSetLayout, textureSetLayout, textureSetLayout, textureSetLayout};
    VkPushConstantRange canvasRange = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(CanvasConstants)};

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 4,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &canvasRange,
    };
    CheckVulkan(vkCreatePipelineLayout(context.Device(), &layoutInfo, nullptr, &layout), "creating the pipeline layout");

    VkShaderModule vertexShader = LoadShader(context, "Quad.vert.spv");
    VkShaderModule fragmentShader = LoadShader(context, "Quad.frag.spv");

    pipelines[static_cast<std::size_t>(BlendMode::StandardAlpha)] = MakePipeline(BlendMode::StandardAlpha, vertexShader, fragmentShader);
    pipelines[static_cast<std::size_t>(BlendMode::Replace)] = MakePipeline(BlendMode::Replace, vertexShader, fragmentShader);

    vkDestroyShaderModule(context.Device(), vertexShader, nullptr);
    vkDestroyShaderModule(context.Device(), fragmentShader, nullptr);
}

QuadPipeline::~QuadPipeline()
{
    VkDevice device = context.Device();

    for (VkPipeline pipeline : pipelines)
        vkDestroyPipeline(device, pipeline, nullptr);

    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, drawDataSetLayout, nullptr);
}

VkPipeline QuadPipeline::PipelineFor(BlendMode blendMode) const
{
    return pipelines[static_cast<std::size_t>(blendMode)];
}

VkPipelineLayout QuadPipeline::Layout() const
{
    return layout;
}

VkDescriptorSetLayout QuadPipeline::DrawDataSetLayout() const
{
    return drawDataSetLayout;
}

VkDescriptorSetLayout QuadPipeline::TextureSetLayout() const
{
    return textureSetLayout;
}

VkPipeline QuadPipeline::MakePipeline(BlendMode blendMode, VkShaderModule vertexShader, VkShaderModule fragmentShader) const
{
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShader,
            .pName = ShaderEntryPoint},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader,
            .pName = ShaderEntryPoint},
    };

    // There are no vertex buffers. The vertex shader reads the corners of the quad from the draw data.
    VkPipelineVertexInputStateCreateInfo vertexInput = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    // The viewport and the scissor are set while the commands are recorded.
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Standard alpha blending: result = source * source alpha + destination * (1 - source alpha), for the alpha too.
    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = blendMode == BlendMode::StandardAlpha ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo colorBlend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates,
    };

    // The pipeline draws into a picture without a render pass. There is no depth buffer.
    VkFormat colorFormat = PictureFormat;
    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = layout,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    CheckVulkan(vkCreateGraphicsPipelines(context.Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "creating a pipeline");
    return pipeline;
}

}
