//
// Created by PC on 2026/6/9.
//

#include "vk_compute.h"

#include <spdlog/spdlog.h>
#include <vector>

#include "vk_device.h"
#include "vk_memory.h"

using namespace rtfs2d;

ComputeContext::ComputeContext(DeviceManager& dm): dm_(&dm) {
    CreateVelocityBuffers();
    CreateDescriptorSets();
    advection_solver_ = std::make_unique<AdvectionSolver>(dm, *this);
    jacobi_solver_ = std::make_unique<JacobiSolver>(dm, *this);
    divergence_solver_ = std::make_unique<DivergenceSolver>(dm, *this);
    projection_solver_ = std::make_unique<ProjectionSolver>(dm, *this);
}

void ComputeContext::RecordAndSubmit(const vk::raii::Queue& queue) {
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    auto& cb = cbs[0];
    // 录制模拟任务
    RecordFluidStepCommands(cb);
    // 提交与等待
    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    queue.submit(si);
    queue.waitIdle();
}

void ComputeContext::CreateVelocityBuffers() {
    velocity_buffers_.resize(4);
    velocity_memories_.resize(4);
    std::vector host_data(compute_cell_count_, 0.0f);
    for (int i = 0; i < 4; ++i) {
        auto [buf, mem] = AllocateBuffer(dm_->device(), dm_->physical_device(), compute_buf_size_,
            vk::BufferUsageFlagBits::eStorageBuffer
                | vk::BufferUsageFlagBits::eTransferSrc
                | vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
        velocity_buffers_[i] = std::move(buf);
        velocity_memories_[i] = std::move(mem);
        UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
            dm_->graphics_queue(), host_data, *velocity_buffers_[i]);
    }
}

void ComputeContext::CreateDescriptorSets() {
    DescriptorSetBuilder dsb(dm_->device());
    for (int i = 0; i < 4; ++i) {
        dsb.AddStorageBufferBinding(i, vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment);
    }
    auto [ds_layout, ds_pool, ds_set] = dsb.build();
    descriptor_set_layout_ = std::move(ds_layout);
    descriptor_pool_ = std::move(ds_pool);
    descriptor_set_ = std::move(ds_set);
}

void ComputeContext::RecordFluidStepCommands(const vk::raii::CommandBuffer& cb) {
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    DescriptorSetBuilder dsb(dm_->device());

    // 平流u分量 u_src, u_src -> u_dst
    BindBuffer(dsb, eUSrc, 0);
    BindBuffer(dsb, eUDst, 1);
    advection_solver_->RecordCommands(cb);
    SwapBuffer(eUSrc, eUDst);

    // 平流v分量 u_src, v_src -> v_dst
    BindBuffer(dsb, eVSrc, 0);
    BindBuffer(dsb, eVDst, 1);
    advection_solver_->RecordCommands(cb);
    SwapBuffer(eVSrc, eVDst);

    // 扩散迭代（u 和 v 各做一次雅可比迭代）
    float viscosity = 0.0f;
    float dx = grid_params_.dx;
    float alpha = viscosity * 0.016f / (dx * dx);
    float beta = 4.0f + alpha;
    for (int iter = 0; iter < 20; ++iter) {
        // u_src -> u_dst
        BindBuffer(dsb, eUSrc, 0);
        BindBuffer(dsb, eUSrc, 1);
        BindBuffer(dsb, eUDst, 3);
        jacobi_solver_->RecordCommands(cb, alpha, beta);
        SwapBuffer(eUSrc, eUDst);

        // v_src -> v_dst
        BindBuffer(dsb, eVSrc, 0);
        BindBuffer(dsb, eVSrc, 1);
        BindBuffer(dsb, eVDst, 3);
        jacobi_solver_->RecordCommands(cb, alpha, beta);
        SwapBuffer(eVSrc, eVDst);
    }

    // 散度计算 u_src, v_src -> u_dst
    BindBuffer(dsb, eUDst, 1);
    BindBuffer(dsb, eUSrc, 2);
    BindBuffer(dsb, eVSrc, 3);
    divergence_solver_->RecordCommands(cb);

    // 压力求解迭代（雅可比迭代解泊松方程） v_src(输入场), u_dst(上一步算的散度) -> v_dst(输出场)
    float alpha_p = -(dx * dx);
    for (int iter = 0; iter < 40; ++iter) {
        float beta_p = 4.0f;
        BindBuffer(dsb, eVSrc, 0);
        BindBuffer(dsb, eUDst, 1);
        BindBuffer(dsb, eVDst, 3);
        jacobi_solver_->RecordCommands(cb, alpha_p, beta_p);
        SwapBuffer(eVSrc, eVDst);
    }

    // 压力投影
    BindBuffer(dsb, eVSrc, 0);
    projection_solver_->RecordCommands(cb);
    cb.end();
}
