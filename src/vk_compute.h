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
#include "vk_descriptor.h"

namespace rtfs2d {

class DeviceManager;

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm);
    void RecordAndSubmit(const vk::raii::Queue& queue);

    enum BufferIndex {
        eUSrc = 0,  //当前时间步速度u分量，平流/雅可比迭代的输入
        eUDst = 1,  //速度u分量的计算结果
        eVSrc = 2,  //当前时间步速度v分量
        eVDst = 3   //速度v分量的计算结果
    };

    const vk::raii::Buffer& VelocityBuffer(BufferIndex idx) const {
        return *velocity_buffers_[static_cast<int>(idx)];
    }
    const vk::raii::DeviceMemory& VelocityMemory(BufferIndex idx) const {
        return *velocity_memories_[static_cast<int>(idx)];
    }

    // getter
    const vk::raii::DescriptorSetLayout& descriptor_set_layout() const { return *descriptor_set_layout_; }
    const vk::raii::DescriptorSet& descriptor_set() const { return *descriptor_set_; }
    const GridParams& grid_params() const { return grid_params_; }
    int cell_count() const { return compute_cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }
    const vk::raii::PipelineLayout& pipeline_layout() const { return *pipeline_layout_; }
private:
    DeviceManager* dm_;

    // { u_src, u_dst, v_src, v_dst }
    std::vector<std::unique_ptr<vk::raii::Buffer>> velocity_buffers_;
    std::vector<std::unique_ptr<vk::raii::DeviceMemory>> velocity_memories_;
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::unique_ptr<vk::raii::DescriptorSet> descriptor_set_;

    std::unique_ptr<AdvectionSolver> advection_solver_;
    std::unique_ptr<JacobiSolver> jacobi_solver_;
    std::unique_ptr<DivergenceSolver> divergence_solver_;
    std::unique_ptr<ProjectionSolver> projection_solver_;

    const GridParams grid_params_{128, 128, 1.0f, 1.0f};
    const int compute_cell_count_ = grid_params_.TotalCells();
    const vk::DeviceSize compute_buf_size_ = compute_cell_count_ * sizeof(float);

    void CreateVelocityBuffers();
    void CreateDescriptorSets();

    void BindBuffer(DescriptorSetBuilder& dsb, BufferIndex buf_idx, int desc_idx) const {
        dsb.WriteBuffer(*descriptor_set_, desc_idx, VelocityBuffer(buf_idx));
    }

    void SwapBuffer(BufferIndex b1, BufferIndex b2) {
        std::swap(velocity_buffers_[b1], velocity_buffers_[b2]);
        std::swap(velocity_memories_[b1], velocity_memories_[b2]);
    }
    void RecordFluidStepCommands(const vk::raii::CommandBuffer& cb);
};

}

#endif //RTFS2D_VK_COMPUTE_H