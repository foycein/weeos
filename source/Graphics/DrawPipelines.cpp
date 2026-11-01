#include "Graphics/DrawPipelines.h"

#include "Formats/FileBytes.h"
#include "Graphics/GraphicsError.h"
#include "Graphics/VulkanSupport.h"
#include "Platform/FileLocations.h"

#include <array>
#include <filesystem>
#include <vector>

namespace Graphics
{

namespace
{

constexpr const char* ShaderEntryPoint = "main";

constexpr VkColorComponentFlags AllColorComponents =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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

// How a blend mode mixes the new pixel (source) with the picture (destination): result = source * sourceFactor +
// destination * destinationFactor, for the colors and for the alpha. GraphicsDevice.h says what each mode does.
struct BlendFactors
{
    VkBlendFactor sourceColor;
    VkBlendFactor destinationColor;
    VkBlendFactor sourceAlpha;
    VkBlendFactor destinationAlpha;
};

BlendFactors FactorsOf(BlendMode blendMode)
{
    switch (blendMode)
    {
    case BlendMode::StandardAlpha:
    case BlendMode::Replace:
        return {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA};
    case BlendMode::AddSourceAlpha:
        return {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE};
    case BlendMode::Over:
        return {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA};
    case BlendMode::Behind:
        return {VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ONE};
    }
    throw GraphicsError("A blend mode that does not exist");
}

// Replace turns blending off, so the source replaces what was there.
VkPipelineColorBlendAttachmentState MakeBlendAttachment(BlendMode blendMode)
{
    BlendFactors factors = FactorsOf(blendMode);

    return {
        .blendEnable = blendMode == BlendMode::Replace ? VK_FALSE : VK_TRUE,
        .srcColorBlendFactor = factors.sourceColor,
        .dstColorBlendFactor = factors.destinationColor,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = factors.sourceAlpha,
        .dstAlphaBlendFactor = factors.destinationAlpha,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = AllColorComponents,
    };
}

VkDescriptorSetLayout MakeDrawDataSetLayout(const VulkanContext& context)
{
    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (std::uint32_t binding = 0; binding < DrawDataBindingCount; ++binding)
        bindings.push_back({.binding = binding, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = stages});

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = DrawDataBindingCount,
        .pBindings = bindings.data(),
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

DrawPipelines::DrawPipelines(const VulkanContext& graphicsContext)
    : context(graphicsContext)
{
    drawDataSetLayout = MakeDrawDataSetLayout(context);
    textureSetLayout = MakeTextureSetLayout(context);

    std::array<VkDescriptorSetLayout, DescriptorSetCount> setLayouts = {
        drawDataSetLayout, textureSetLayout, textureSetLayout, textureSetLayout};
    VkPushConstantRange canvasRange = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(CanvasConstants)};

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = DescriptorSetCount,
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &canvasRange,
    };
    CheckVulkan(vkCreatePipelineLayout(context.Device(), &layoutInfo, nullptr, &layout), "creating the pipeline layout");

    VkShaderModule quadShader = LoadShader(context, "Quad.vert.spv");
    VkShaderModule meshShader = LoadShader(context, "Mesh.vert.spv");
    VkShaderModule fragmentShader = LoadShader(context, "Quad.frag.spv");

    for (std::size_t modeNumber = 0; modeNumber < BlendModeCount; ++modeNumber)
    {
        auto blendMode = static_cast<BlendMode>(modeNumber);
        quadPipelines[modeNumber] = MakePipeline(Shape::Quad, blendMode, quadShader, fragmentShader);
        meshPipelines[modeNumber] = MakePipeline(Shape::Mesh, blendMode, meshShader, fragmentShader);
    }

    vkDestroyShaderModule(context.Device(), quadShader, nullptr);
    vkDestroyShaderModule(context.Device(), meshShader, nullptr);
    vkDestroyShaderModule(context.Device(), fragmentShader, nullptr);
}

DrawPipelines::~DrawPipelines()
{
    VkDevice device = context.Device();

    for (VkPipeline pipeline : quadPipelines)
        vkDestroyPipeline(device, pipeline, nullptr);
    for (VkPipeline pipeline : meshPipelines)
        vkDestroyPipeline(device, pipeline, nullptr);

    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, drawDataSetLayout, nullptr);
}

VkPipeline DrawPipelines::QuadPipelineFor(BlendMode blendMode) const
{
    return quadPipelines[static_cast<std::size_t>(blendMode)];
}

VkPipeline DrawPipelines::MeshPipelineFor(BlendMode blendMode) const
{
    return meshPipelines[static_cast<std::size_t>(blendMode)];
}

VkPipelineLayout DrawPipelines::Layout() const
{
    return layout;
}

VkDescriptorSetLayout DrawPipelines::DrawDataSetLayout() const
{
    return drawDataSetLayout;
}

VkDescriptorSetLayout DrawPipelines::TextureSetLayout() const
{
    return textureSetLayout;
}

VkPipeline DrawPipelines::MakePipeline(Shape shape, BlendMode blendMode, VkShaderModule vertexShader, VkShaderModule fragmentShader) const
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

    // There are no vertex buffers. The vertex shaders read the corners from the draw data.
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

    // Quads are never culled. For meshes the culling is set before each mesh; the front of a triangle is the side from which
    // its corners go around clockwise in the picture.
    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Quads leave the depth alone. For meshes the test and the writing are set before each mesh.
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    };

    VkPipelineColorBlendAttachmentState blendAttachment = MakeBlendAttachment(blendMode);
    VkPipelineColorBlendStateCreateInfo colorBlend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    if (shape == Shape::Mesh)
    {
        dynamicStates.push_back(VK_DYNAMIC_STATE_CULL_MODE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
    }
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    // The pipeline draws into a picture without a render pass.
    VkFormat colorFormat = PictureFormat;
    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = DepthFormat,
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
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = layout,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    CheckVulkan(vkCreateGraphicsPipelines(context.Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "creating a pipeline");
    return pipeline;
}

}
