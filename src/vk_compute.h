//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_COMPUTE_H
#define RTFS2D_VK_COMPUTE_H

#include <vulkan/vulkan_raii.hpp>
#include "grid.h"

namespace rtfs2d {

class DeviceManager;

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm);
    void RecordAndSubmit(const vk::raii::Queue& queue);
    void Verify() const;

    // getter
    const vk::raii::Buffer& scalar_field_buffer() const { return *scalar_field_buffer_; }
    const vk::raii::DescriptorSetLayout& descriptor_set_layout() const { return *descriptor_set_layout_; }
    const vk::raii::DescriptorSet& descriptor_set() const { return *descriptor_set_; }
    const GridParams& grid_params() const { return grid_params_; }
    int cell_count() const { return compute_cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }

private:
    DeviceManager* dm_;

    std::unique_ptr<vk::raii::Buffer> scalar_field_buffer_;
    std::unique_ptr<vk::raii::DeviceMemory> scalar_field_memory_;
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> compute_pipeline_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::unique_ptr<vk::raii::DescriptorSet> descriptor_set_;
    std::unique_ptr<vk::raii::CommandBuffer> compute_command_buffer_;
    bool verified_ = false;
    const GridParams grid_params_{128, 128, 1.0f, 1.0f};
    const int compute_cell_count_ = grid_params_.TotalCells();
    const vk::DeviceSize compute_buf_size_ = compute_cell_count_ * sizeof(float);

    void CreateStorageBuffer();
    void CreateDescriptorSets();
    void CreateComputePipeline();
    void RecordComputeCommands();
};

}

#endif //RTFS2D_VK_COMPUTE_H