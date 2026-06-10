//
// Created by PC on 2026/6/10.
//

#include "divergence_solver.h"
#include "vk_device.h"
#include "vk_compute.h"

using namespace rtfs2d;

DivergenceSolver::DivergenceSolver(DeviceManager &dm, ComputeContext &cc): dm_(&dm), cc_(&cc) {
    pipeline_ = dm_->CreateComputePipelineFromFile(
        cc.pipeline_layout(), "shaders/diverge.comp.spv");
}

void DivergenceSolver::RecordCommands(const vk::raii::CommandBuffer &cb) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *cc_->descriptor_set(), nullptr);
    int group_count = (cc_->cell_count() + 127) / 128;
    cb.dispatch(group_count, 1, 1);
    vk::BufferMemoryBarrier barrier{};
    barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
        .setBuffer(*cc_->VelocityBuffer(ComputeContext::eUDst))
        .setSize(cc_->buf_size())
        .setOffset(0)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader,
        {}, nullptr, barrier, nullptr);
}
