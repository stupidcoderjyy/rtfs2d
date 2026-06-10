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
    dsb.WriteBuffer(*descriptor_set_, 0, *scalar_field_buffer_);
    dsb.WriteBuffer(*descriptor_set_,1, *velocity_buffers_[0]);
    dsb.WriteBuffer(*descriptor_set_,2, *velocity_buffers_[2]);
    dsb.WriteBuffer(*descriptor_set_,3, *velocity_buffers_[1]);
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

void ComputeContext::RecordAndSubmit(const vk::raii::Queue& queue) {
    vk::SubmitInfo c_si{};
    c_si.setCommandBuffers(**compute_command_buffer_);
    queue.submit(c_si);
    queue.waitIdle();
    if (!verified_) {
        Verify();
        verified_ = true;
    }
}

void ComputeContext::Verify() const {
    vk::BufferCreateInfo ci{};
    ci.setSize(compute_buf_size_)
        .setUsage(vk::BufferUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive);
    auto staging_buf = dm_->device().createBuffer(ci);

    auto mr = staging_buf.getMemoryRequirements();
    auto mti = FindMemoryType(dm_->physical_device(), mr.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo m_ai{};
    m_ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    auto staging_mem = dm_->device().allocateMemory(m_ai);
    staging_buf.bindMemory(*staging_mem, 0);

    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    auto& cb = cbs.front();
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::BufferCopy copy{};
    copy.setSrcOffset(0)
        .setSize(compute_buf_size_);
    cb.copyBuffer(**scalar_field_buffer_, *staging_buf, copy);
    cb.end();

    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    dm_->graphics_queue().submit(si);
    dm_->graphics_queue().waitIdle();

    auto* p_res = static_cast<float*>(staging_mem.mapMemory(0, compute_buf_size_));
    bool pass = true;
    for (int k = 0; k < compute_cell_count_; ++k) {
        int i = k % grid_params_.nx, j = k / grid_params_.nx;
        float fx = static_cast<float>(i) / static_cast<float>(grid_params_.nx);
        float fy = static_cast<float>(j) / static_cast<float>(grid_params_.ny);
        float r = std::sqrt((fx - 0.5f) * (fx - 0.5f) + (fy - 0.5f) * (fy - 0.5f)) * 2.0f;
        float expected = std::sin(r * 20.0f) * 0.5f + 0.5f;
        if (float delta = std::abs(p_res[k] - expected); delta > 1e-5f) {
            spdlog::warn("Value mismatch at ({}, {}), expected: {}, actual: {}",
                i, j, expected, p_res[k]);
            pass = false;
            break;
        }
    }
    if (pass) {
        spdlog::info("All values match within tolerance");
    }
    staging_mem.unmapMemory();
}