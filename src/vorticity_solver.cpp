//
// Created by PC on 2026/6/11.
//

#include "vorticity_solver.h"
#include "vk_device.h"
#include "vk_compute.h"

using namespace rtfs2d;

VorticitySolver::VorticitySolver(DeviceManager& dm, ComputeContext& cc): dm_(&dm), cc_(&cc) {
    vk::PushConstantRange push_range{};
    push_range.setStageFlags(vk::ShaderStageFlagBits::eCompute)
        .setOffset(0)
        .setSize(sizeof(float));

    vk::PipelineLayoutCreateInfo layout_ci{};
    layout_ci.setSetLayoutCount(1)
        .setPSetLayouts(&*cc_->descriptor_set_layout())
        .setPushConstantRangeCount(1)
        .setPPushConstantRanges(&push_range);

    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(layout_ci));

    pipeline_ = dm_->CreateComputePipelineFromFile(
        *pipeline_layout_, "shaders/vorticity.comp.spv");
}

void VorticitySolver::RecordCommands(const vk::raii::CommandBuffer& cb,
        const vk::raii::DescriptorSet& ds,
        float epsilon) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, **pipeline_layout_,
        0, *ds, nullptr);
    cb.pushConstants<float>(**pipeline_layout_, vk::ShaderStageFlagBits::eCompute,
        0, {epsilon});
    int group_count = (cc_->cell_count() + 127) / 128;
    cb.dispatch(group_count, 1, 1);
}