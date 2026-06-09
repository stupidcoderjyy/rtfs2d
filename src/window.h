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
    bool debug_enabled_;
    size_t current_frame_ = 0;

    std::unique_ptr<vk::raii::ShaderModule> compute_shader_module_;
    std::unique_ptr<vk::raii::Buffer> scalar_field_buffer_;
    std::unique_ptr<vk::raii::DeviceMemory> scalar_field_memory_;
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> compute_pipeline_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::unique_ptr<vk::raii::DescriptorSet> descriptor_set_;
    std::unique_ptr<vk::raii::CommandBuffer> compute_command_buffer_;
    bool compute_verified_ = false;
    const GridParams grid_params_{128, 128, 1.0f, 1.0f};
    const int compute_cell_count_ = grid_params_.TotalCells();
    const vk::DeviceSize compute_buf_size_ = compute_cell_count_ * sizeof(float);

    std::unique_ptr<vk::raii::ShaderModule> vert_shader_module_;
    std::unique_ptr<vk::raii::ShaderModule> frag_shader_module_;
    std::unique_ptr<vk::raii::PipelineLayout> graphics_pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> graphics_pipeline_;

    void RecordCommands(const vk::raii::CommandBuffer& cb, uint32_t img_idx) const;

    std::unique_ptr<vk::raii::ShaderModule> LoadShader(const std::string& path) const;
    void CreateStorageBuffer();
    void CreatePipelineLayout();
    void CreateComputePipeline();
    void RecordComputeCommands();
    void VerifyFieldData() const;

    void CreateGraphicsPipeline();
};

}

#endif //RTFS2D_WINDOW_H
