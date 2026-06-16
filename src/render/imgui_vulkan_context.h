//
// Created by PC on 2026/6/16.
//

#ifndef RTFS2D_IMGUI_VULKAN_CONTEXT_H
#define RTFS2D_IMGUI_VULKAN_CONTEXT_H

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

namespace rtfs2d {

class DeviceManager;

class ImGuiVulkanContext {
public:
    ImGuiVulkanContext(GLFWwindow* window, DeviceManager& dm, vk::RenderPass render_pass, uint32_t swapchain_image_count);

    static void BeginFrame();
    static void EndFrame(const vk::raii::CommandBuffer& cb);
    static void Shutdown();
private:
    std::unique_ptr<vk::raii::DescriptorPool> imgui_descriptor_pool_;
};

}

#endif //RTFS2D_IMGUI_VULKAN_CONTEXT_H
