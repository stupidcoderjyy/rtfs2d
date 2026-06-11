//
// Created by PC on 2026/6/11.
//

#include "pressure_bc_solver.h"
#include "vk_device.h"
#include "vk_compute.h"

using namespace rtfs2d;

PressureBCSolver::PressureBCSolver(DeviceManager& dm, ComputeContext& cc)
    : dm_(&dm), cc_(&cc) {
    // 复用 ComputeContext 的管线布局（已包含所有 binding 布局）
    pipeline_ = dm_->CreateComputePipelineFromFile(
        cc.pipeline_layout(), "shaders/pressure_bc.comp.spv");
}

void PressureBCSolver::RecordCommands(const vk::raii::CommandBuffer& cb,
        const vk::raii::DescriptorSet& ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    int group_count = (cc_->cell_count() + 127) / 128; // 128 = local_size_x
    cb.dispatch(group_count, 1, 1);
}