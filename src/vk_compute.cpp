//
// Created by PC on 2026/6/9.
//

#include "vk_compute.h"

#include <spdlog/spdlog.h>
#include <vector>

#include "vk_device.h"
#include "vk_memory.h"
#include "vk_descriptor.h"

using namespace rtfs2d;

ComputeContext::ComputeContext(DeviceManager& dm): dm_(&dm) {
    CreateStorageBuffer();
    CreateVelocityBuffers();
    CreateDescriptorSets();
    CreateComputePipeline();
    RecordComputeCommands();
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

void ComputeContext::CreateStorageBuffer() {
    auto [buf, mem] = AllocateBuffer(dm_->device(), dm_->physical_device(), compute_buf_size_,
        vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    scalar_field_buffer_ = std::move(buf);
    scalar_field_memory_ = std::move(mem);
    std::vector<float> host_data(compute_cell_count_, 0.0f);
    UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
        dm_->graphics_queue(), host_data, *scalar_field_buffer_);
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
    // 1和2最终会动态绑定到u和v，现在只是占位
    dsb.WriteBuffer(*descriptor_set_, 0, *scalar_field_buffer_);
    dsb.WriteBuffer(*descriptor_set_,1, VelocityBuffer(eUDst));
    dsb.WriteBuffer(*descriptor_set_,2, VelocityBuffer(eUSrc));
    dsb.WriteBuffer(*descriptor_set_,3, VelocityBuffer(eVSrc));
}

void ComputeContext::CreateComputePipeline() {
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&**descriptor_set_layout_);
    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(ci));
    compute_pipeline_ = dm_->CreateComputePipelineFromFile(
        *pipeline_layout_, "shaders/compute.comp.spv");
}

void ComputeContext::RecordComputeCommands() {
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    compute_command_buffer_ = std::make_unique<vk::raii::CommandBuffer>(std::move(cbs[0]));
    auto& cb = *compute_command_buffer_;
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **compute_pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, **pipeline_layout_, 0, **descriptor_set_, nullptr);
    int group_count = (compute_cell_count_ + 127) / 128;
    cb.dispatch(group_count, 1, 1);
    vk::BufferMemoryBarrier barrier{};
    barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
        .setDstAccessMask(vk::AccessFlagBits::eHostRead)
        .setBuffer(**scalar_field_buffer_)
        .setSize(compute_buf_size_)
        .setOffset(0)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eHost,
        {}, nullptr, barrier, nullptr);
    cb.end();
}

// 将输出场设置为输入场
void ComputeContext::SwapVelocityBuffers() {
    std::swap(velocity_buffers_[eUSrc], velocity_buffers_[eUDst]);
    std::swap(velocity_memories_[eUSrc], velocity_memories_[eUDst]);
    std::swap(velocity_buffers_[eVSrc], velocity_buffers_[eVDst]);
    std::swap(velocity_memories_[eVSrc], velocity_memories_[eVDst]);
    DescriptorSetBuilder dsb(dm_->device());
    dsb.WriteBuffer(*descriptor_set_,1, VelocityBuffer(eUDst));
    dsb.WriteBuffer(*descriptor_set_,2, VelocityBuffer(eUSrc));
    dsb.WriteBuffer(*descriptor_set_,3, VelocityBuffer(eVSrc));
}

void ComputeContext::RecordFluidStepCommands(const vk::raii::CommandBuffer& cb) {
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    DescriptorSetBuilder dsb(dm_->device());

    // 平流u分量 u_src, u_src -> u_dst
    dsb.WriteBuffer(*descriptor_set_, 0, VelocityBuffer(eUSrc));
    dsb.WriteBuffer(*descriptor_set_, 1, VelocityBuffer(eUDst));
    advection_solver_->RecordCommands(cb);
    SwapVelocityBuffers();

    // 平流v分量 u_src, v_src -> v_dst
    dsb.WriteBuffer(*descriptor_set_, 0, VelocityBuffer(eVSrc));
    dsb.WriteBuffer(*descriptor_set_, 1, VelocityBuffer(eVDst));
    advection_solver_->RecordCommands(cb);
    SwapVelocityBuffers();

    // 扩散迭代（u 和 v 各做一次雅可比迭代）
    float viscosity = 0.0f;
    float dx = grid_params_.dx;
    float alpha = viscosity * 0.016f / (dx * dx);
    float beta = 4.0f + alpha;
    for (int iter = 0; iter < 20; ++iter) {
        // u_src -> u_dst
        dsb.WriteBuffer(*descriptor_set_, 0, VelocityBuffer(eUSrc));
        dsb.WriteBuffer(*descriptor_set_, 1, VelocityBuffer(eUSrc));
        dsb.WriteBuffer(*descriptor_set_, 3, VelocityBuffer(eUDst));
        jacobi_solver_->RecordCommands(cb, alpha, beta);
        SwapVelocityBuffers();

        // v_src -> v_dst
        dsb.WriteBuffer(*descriptor_set_, 0, VelocityBuffer(eVSrc));
        dsb.WriteBuffer(*descriptor_set_, 1, VelocityBuffer(eVSrc));
        dsb.WriteBuffer(*descriptor_set_, 3, VelocityBuffer(eVDst));
        jacobi_solver_->RecordCommands(cb, alpha, beta);
        SwapVelocityBuffers();
    }

    // 散度计算 u_src, v_src -> u_dst
    dsb.WriteBuffer(*descriptor_set_, 1, VelocityBuffer(eUDst));
    divergence_solver_->RecordCommands(cb);

    // 压力求解迭代（雅可比迭代解泊松方程） scalar_field_buffer_, u_dst(上一步算的散度) -> scalar_field_buffer_
    float alpha_p = -(dx * dx);
    dsb.WriteBuffer(*descriptor_set_, 0, *scalar_field_buffer_);
    dsb.WriteBuffer(*descriptor_set_, 1, VelocityBuffer(eUDst));
    dsb.WriteBuffer(*descriptor_set_, 3, *scalar_field_buffer_);
    for (int iter = 0; iter < 40; ++iter) {
        float beta_p = 4.0f;
        jacobi_solver_->RecordCommands(cb, alpha_p, beta_p);
    }

    // 压力投影
    dsb.WriteBuffer(*descriptor_set_, 0, *scalar_field_buffer_);
    projection_solver_->RecordCommands(cb);
    cb.end();
}