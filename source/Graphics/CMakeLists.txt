# The Vulkan SDK is found through the VULKAN_SDK variable. This is where it is installed on the development PC.
if(NOT DEFINED ENV{VULKAN_SDK})
    set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.4.363.0")
endif()
find_package(Vulkan REQUIRED COMPONENTS glslc)

add_library(Graphics STATIC
    CanvasMapping.cpp
    ConsoleVideo.cpp
    DepthBuffers.cpp
    DrawData.cpp
    DrawPipelines.cpp
    FramePacer.cpp
    FrameResources.cpp
    GraphicsBuffer.cpp
    GraphicsImage.cpp
    ImageMemoryPool.cpp
    MaterialCheck.cpp
    Matrix4.cpp
    OneTimeCommands.cpp
    RangeAllocator.cpp
    StageList.cpp
    StoredMeshes.cpp
    Swapchain.cpp
    TextureStore.cpp
    VulkanContext.cpp
    VulkanGraphicsDevice.cpp
    VulkanSupport.cpp
)

target_compile_definitions(Graphics PUBLIC VK_USE_PLATFORM_WIN32_KHR)
target_link_libraries(Graphics PUBLIC Formats Platform Vulkan::Vulkan)

# Compiles the shaders to SPIR-V files in the folder of a program, which is where the program looks for them.
# Call this for every program that draws with the Vulkan device.
function(CompileGraphicsShaders programName)
    set(shaderFolder ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Shaders)
    set(shaderNames Quad.vert Quad.frag Mesh.vert)
    set(shaderIncludes ${shaderFolder}/DrawData.glsl ${shaderFolder}/StageConstants.glsl)

    foreach(shaderName ${shaderNames})
        set(spirvFile ${CMAKE_CURRENT_BINARY_DIR}/${shaderName}.spv)
        add_custom_command(
            OUTPUT ${spirvFile}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3 -o ${spirvFile} ${shaderFolder}/${shaderName}
            DEPENDS ${shaderFolder}/${shaderName} ${shaderIncludes}
        )
        list(APPEND spirvFiles ${spirvFile})
    endforeach()

    add_custom_target(${programName}Shaders ALL DEPENDS ${spirvFiles})
    add_dependencies(${programName} ${programName}Shaders)
endfunction()
