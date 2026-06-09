//
// Created by PC on 2026/6/5.
//

#ifndef RTFS2D_WINDOW_H
#define RTFS2D_WINDOW_H
#include <string>
#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <grid.h>
#include "vk_swapchain.h"
#include "vk_device.h"
#include "vk_compute.h"

namespace rtfs2d {

class Window {
public:
    Window(int width, int height, std::string title, bool debug_enabled = true);
    void Show();
private:
    int width_, height_;
    std::string title_;
    GLFWwindow* window_{};
    std::unique_ptr<DeviceManager> device_manager_;
    std::unique_ptr<SwapchainContext> swapchain_ctx_;
    std::unique_ptr<ComputeContext> compute_ctx_;
    bool debug_enabled_;
    size_t current_frame_ = 0;

    std::unique_ptr<vk::raii::PipelineLayout> graphics_pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> graphics_pipeline_;

    void RecordCommands(const vk::raii::CommandBuffer& cb, uint32_t img_idx) const;

    std::unique_ptr<vk::raii::ShaderModule> LoadShader(const std::string& path) const;

    void CreateGraphicsPipeline();
};

}

#endif //RTFS2D_WINDOW_H
