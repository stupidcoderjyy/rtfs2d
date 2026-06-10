//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_COMPUTE_H
#define RTFS2D_VK_COMPUTE_H

#include <vulkan/vulkan_raii.hpp>
#include "grid.h"
#include "jacobi_solver.h"
#include "advection_solver.h"
#include "divergence_solver.h"
#include "projection_solver.h"

namespace rtfs2d {

class DeviceManager;

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm);
    void RecordAndSubmit(const vk::raii::Queue& queue);

    enum VelocityBufferIndex {
        eUSrc = 0,  //当前时间步速度u分量，平流/雅可比迭代的输入
        eUDst = 1,  //速度u分量的计算结果
        eVSrc = 2,  //当前时间步速度v分量
        eVDst = 3   //速度v分量的计算结果
    };

    const vk::raii::Buffer& VelocityBuffer(VelocityBufferIndex idx) const {
        return *velocity_buffers_[static_cast<int>(idx)];
    }
    const vk::raii::DeviceMemory& VelocityMemory(VelocityBufferIndex idx) const {
        return *velocity_memories_[static_cast<int>(idx)];
    }

    // getter
    const vk::raii::Buffer& scalar_field_buffer() const { return *scalar_field_buffer_; }
    const vk::raii::DescriptorSetLayout& descriptor_set_layout() const { return *descriptor_set_layout_; }
    const vk::raii::DescriptorSet& descriptor_set() const { return *descriptor_set_; }
    const GridParams& grid_params() const { return grid_params_; }
    int cell_count() const { return compute_cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }
    const vk::raii::PipelineLayout& pipeline_layout() const { return *pipeline_layout_; }
private:
    DeviceManager* dm_;

    std::unique_ptr<vk::raii::Buffer> scalar_field_buffer_;
    std::unique_ptr<vk::raii::DeviceMemory> scalar_field_memory_;
    // { u_src, u_dst, v_src, v_dst }
    std::vector<std::unique_ptr<vk::raii::Buffer>> velocity_buffers_;
    std::vector<std::unique_ptr<vk::raii::DeviceMemory>> velocity_memories_;
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> compute_pipeline_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::unique_ptr<vk::raii::DescriptorSet> descriptor_set_;
    std::unique_ptr<vk::raii::CommandBuffer> compute_command_buffer_;

    std::unique_ptr<AdvectionSolver> advection_solver_;
    std::unique_ptr<JacobiSolver> jacobi_solver_;
    std::unique_ptr<DivergenceSolver> divergence_solver_;
    std::unique_ptr<ProjectionSolver> projection_solver_;

    const GridParams grid_params_{128, 128, 1.0f, 1.0f};
    const int compute_cell_count_ = grid_params_.TotalCells();
    const vk::DeviceSize compute_buf_size_ = compute_cell_count_ * sizeof(float);

    void CreateStorageBuffer();
    void CreateVelocityBuffers();
    void CreateDescriptorSets();
    void CreateComputePipeline();
    void RecordComputeCommands();

    void SwapVelocityBuffers();
    void RecordFluidStepCommands(const vk::raii::CommandBuffer& cb);
};

}

#endif //RTFS2D_VK_COMPUTE_H