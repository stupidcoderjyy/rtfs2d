//
// Created by PC on 2026/6/10.
//

#include "projection_solver.h"
#include "vk_device.h"
#include "vk_compute.h"

using namespace rtfs2d;

ProjectionSolver::ProjectionSolver(DeviceManager &dm, ComputeContext &cc): dm_(&dm), cc_(&cc) {
    pipeline_ = dm_->CreateComputePipelineFromFile(
        cc.pipeline_layout(), "shaders/project.comp.spv");
}

void ProjectionSolver::RecordCommands(const vk::raii::CommandBuffer &cb,
        const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    int group_count = (cc_->cell_count() + 127) / 128;
    cb.dispatch(group_count, 1, 1);
}
