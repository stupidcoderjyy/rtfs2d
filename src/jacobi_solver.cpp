//
// Created by PC on 2026/6/10.
//

#include "jacobi_solver.h"
#include "vk_device.h"
#include "vk_compute.h"

using namespace rtfs2d;

JacobiSolver::JacobiSolver(DeviceManager &dm, ComputeContext &cc): dm_(&dm), cc_(&cc) {
    vk::PushConstantRange pcr{};
    pcr.setStageFlags(vk::ShaderStageFlagBits::eCompute)
        .setOffset(0)
        .setSize(2 * sizeof(float));
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&*cc_->descriptor_set_layout())
        .setPushConstantRanges(pcr);
    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(ci));
    pipeline_ = dm_->CreateComputePipelineFromFile(
        *pipeline_layout_, "shaders/jacobi.comp.spv");
}

void JacobiSolver::RecordCommands(const vk::raii::CommandBuffer &cb, float alpha, float beta) const {
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, **pipeline_layout_,
        0, *cc_->descriptor_set(), nullptr);
    int group_count = (cc_->cell_count() + 127) / 128;
    cb.pushConstants<float>(**pipeline_layout_,
        vk::ShaderStageFlagBits::eCompute, 0, {alpha, beta});
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
    cb.end();
}
