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

void DivergenceSolver::RecordCommands(const vk::raii::CommandBuffer &cb,
        const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    int group_count = (cc_->cell_count() + 127) / 128;
    cb.dispatch(group_count, 1, 1);
}
