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
#include "vorticity_solver.h"
#include "pressure_bc_solver.h"
#include "vk_boundary.h"

namespace rtfs2d {

class DeviceManager;

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm);
    void RecordAndSubmit(const vk::raii::Queue& queue) const;

    enum DescriptorSetIndex {
        kSetAdvectionU = 0,          // 平流 u 分量
        kSetAdvectionV = 1,          // 平流 v 分量
        kSetDiffusionUEven = 2,      // 扩散 u 分量偶次迭代
        kSetDiffusionUOdd = 3,       // 扩散 u 分量奇次迭代
        kSetDiffusionVEven = 4,      // 扩散 v 分量偶次迭代
        kSetDiffusionVOdd = 5,       // 扩散 v 分量奇次迭代
        kSetDivergence = 6,          // 散度计算
        kSetPressureEven = 7,        // 压力求解偶次迭代
        kSetPressureOdd = 8,         // 压力求解奇次迭代
        kSetProjection = 9,          // 压力投影修正速度
        kSetVorticity = 10,          // 涡量约束
        kSetPressureBc = 11
    };

    void AddBufferMemoryWriteReadBarrier(const vk::raii::CommandBuffer& cb, int buf) const;

    const vk::raii::Buffer& BufferAt(int idx) const {
        return *velocity_buffers_[idx];
    }
    const vk::raii::DeviceMemory& MemoryAt(int idx) const {
        return *velocity_memories_[idx];
    }
    const vk::raii::DescriptorSet& DescriptorSetAt(DescriptorSetIndex idx) const {
        return *descriptor_sets_[idx];
    }

    // getter
    const vk::raii::DescriptorSetLayout& descriptor_set_layout() const { return *descriptor_set_layout_; }
    const GridParams& grid_params() const { return grid_params_; }
    int cell_count() const { return compute_cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }
    const vk::raii::PipelineLayout& pipeline_layout() const { return *pipeline_layout_; }
    BoundaryContext& boundary_ctx() const { return *boundary_ctx_; }
private:
    DeviceManager* dm_;
    // { u_src, u_dst, v_src, v_dst }
    static constexpr int kBindingsSize = 8;
    std::vector<std::unique_ptr<vk::raii::Buffer>> velocity_buffers_;
    std::vector<std::unique_ptr<vk::raii::DeviceMemory>> velocity_memories_;
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::vector<std::unique_ptr<vk::raii::DescriptorSet>> descriptor_sets_;
    std::unique_ptr<AdvectionSolver> advection_solver_;
    std::unique_ptr<JacobiSolver> jacobi_solver_;
    std::unique_ptr<DivergenceSolver> divergence_solver_;
    std::unique_ptr<ProjectionSolver> projection_solver_;
    std::unique_ptr<VorticitySolver> vorticity_solver_;
    std::unique_ptr<PressureBCSolver> pressure_bc_solver_;
    const GridParams grid_params_{128, 128, 1.0f, 1.0f};
    const int compute_cell_count_ = grid_params_.TotalCells();
    const vk::DeviceSize compute_buf_size_ = compute_cell_count_ * sizeof(float);
    std::unique_ptr<BoundaryContext> boundary_ctx_;

    void CreateVelocityBuffers();
    void CreateDescriptorSets();
    void CreatePipelineLayout();
    void RecordFluidStepCommands(const vk::raii::CommandBuffer& cb) const;
    void InitializeVortexField() const; // 初始化流场为涡旋场

    void DebugReadBack(int buffer, const std::string& log_prefix, const std::vector<int>& indexes) const;
};

}

#endif //RTFS2D_VK_COMPUTE_H