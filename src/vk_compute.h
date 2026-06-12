//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_COMPUTE_H
#define RTFS2D_VK_COMPUTE_H

#include <functional>
#include <vulkan/vulkan_raii.hpp>
#include "grid.h"
#include "fluid_solvers.h"
#include "pressure_bc_solver.h"
#include "vk_boundary.h"

namespace rtfs2d {

class DeviceManager;

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm, const GridParams& params);
    void RecordAndSubmit(const vk::raii::Queue& queue) const;

    enum DescriptorSetIndex {
        kSetAdvection,         // 平流
        kSetVorticity,         // 涡量约束
        kSetDiffusionEven,     // 扩散 u 分量偶次迭代
        kSetDiffusionOdd,      // 扩散 u 分量奇次迭代
        kSetDivergence,        // 散度计算
        kSetPressureEven,      // 压力求解偶次迭代
        kSetPressureOdd,       // 压力求解奇次迭代
        kSetProjection,        // 压力投影修正速度
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
    static constexpr int kBindingsSize = 9;
    std::vector<std::unique_ptr<vk::raii::Buffer>> velocity_buffers_;
    std::vector<std::unique_ptr<vk::raii::DeviceMemory>> velocity_memories_;
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::vector<std::unique_ptr<vk::raii::DescriptorSet>> descriptor_sets_;
    std::unique_ptr<FluidSolvers> fluid_solvers_;
    std::unique_ptr<PressureBCSolver> pressure_bc_solver_;
    GridParams grid_params_;
    const int compute_cell_count_;
    const vk::DeviceSize compute_buf_size_;
    std::unique_ptr<BoundaryContext> boundary_ctx_;

    void CreateVelocityBuffers();
    void CreateDescriptorSets();
    void CreatePipelineLayout();
    void RecordFluidStepCommands(const vk::raii::Queue& queue, const vk::raii::CommandBuffer& cb) const;
    void InitializeVortexField() const; // 初始化流场为涡旋场

    void DebugReadBackBuffer(const vk::raii::Buffer& buf, uint32_t size,
        const std::function<void(void* buf, uint32_t len)>& handler) const;
    void DebugReadBackVelocityBuffer(const vk::raii::Queue& queue, const vk::raii::CommandBuffer& cb,
        int buffer, const std::string& log_prefix) const;
    void DebugReadBackVelocityBufferPoints(const vk::raii::Queue& queue, const vk::raii::CommandBuffer& cb,
        int buffer, const std::string& log_prefix, const std::vector<int>& indexes) const;
    void DebugReadBackBoundaryBuffer(BoundaryDirection d) const;
};

}

#endif //RTFS2D_VK_COMPUTE_H