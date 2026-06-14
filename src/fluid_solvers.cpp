//
// Created by PC on 2026/6/11.
//

#include "fluid_solvers.h"

#include "vk_device.h"
#include "vk_compute.h"

namespace rtfs2d {

FluidSolvers::FluidSolvers(DeviceManager &dm, ComputeContext &cc): dm_(&dm), cc_(&cc) {
    pipeline_advection_ = CreateSolverPipeline("shaders/advection.comp.spv");
    pipeline_divergence_ = CreateSolverPipeline("shaders/diverge.comp.spv");
    pipeline_projection_ = CreateSolverPipeline("shaders/project.comp.spv");
    CreateJacobiPipelines();
    CreateVorticitySolverPipeline();
    pipeline_ibm_interpolate_ = CreateSolverPipeline("shaders/ibm_interpolate.comp.spv");
    pipeline_ibm_spread_ = CreateSolverPipeline("shaders/ibm_spread_markers.comp.spv");
    pipeline_ibm_apply_force_ = CreateSolverPipeline("shaders/ibm_apply_force.comp.spv");
    pipeline_ibm_mask_ = CreateSolverPipeline("shaders/ibm_mask.comp.spv");
}

void FluidSolvers::SolveAdvection(
        const vk::raii::CommandBuffer &cb,
        const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_advection_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveDivergence(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_divergence_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveDiffusion(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds,
        float alpha, float beta) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_diffusion_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipeline_layout_diffusion_,
        0, *ds, nullptr);
    cb.pushConstants<float>(*pipeline_layout_diffusion_,
        vk::ShaderStageFlagBits::eCompute, 0, {alpha, beta});
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolvePoisson(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds,
        float alpha, float beta) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_poisson_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipeline_layout_poisson_,
        0, *ds, nullptr);
    cb.pushConstants<float>(*pipeline_layout_poisson_,
        vk::ShaderStageFlagBits::eCompute, 0, {alpha, beta});
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveProjection(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_projection_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveVorticity(const vk::raii::CommandBuffer &cb,
        const vk::raii::DescriptorSet &ds, float epsilon) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_vorticity_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, **pipeline_layout_vorticity_,
        0, *ds, nullptr);
    cb.pushConstants<float>(**pipeline_layout_vorticity_, vk::ShaderStageFlagBits::eCompute,
        0, {epsilon});
    uint32_t group_count = (cc_->cell_count() + 127) / 128;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveIBMInterpolate(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_ibm_interpolate_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->ibm_marker_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveIBMSpread(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_ibm_spread_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->ibm_marker_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::SolveIBMApplyForce(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_ibm_apply_force_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

void FluidSolvers::PrecomputeIBMMask(const vk::raii::CommandBuffer &cb, const vk::raii::DescriptorSet &ds) const {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_ibm_mask_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *cc_->pipeline_layout(),
        0, *ds, nullptr);
    uint32_t group_count = (cc_->cell_count() + kWorkGroupSize - 1) / kWorkGroupSize;
    cb.dispatch(group_count, 1, 1);
}

std::unique_ptr<vk::raii::Pipeline> FluidSolvers::CreateSolverPipeline(const std::string &shader) const {
    const auto& params = cc_->grid_params();
    std::vector<vk::SpecializationMapEntry> mes = {
        {0, 0, sizeof(uint32_t)},   // constant_id 0 -> NX
        {1, sizeof(uint32_t), sizeof(uint32_t)}  // constant_id 1 -> NY
    };
    std::vector<uint8_t> bytes(2 * sizeof(uint32_t));
    memcpy(bytes.data(), &params.nx, sizeof(uint32_t));
    memcpy(bytes.data() + sizeof(uint32_t), &params.ny, sizeof(uint32_t));
    return dm_->CreateComputePipelineFromFile(
        cc_->pipeline_layout(), shader, mes, bytes);
}

void FluidSolvers::CreateJacobiPipelines() {
    vk::PushConstantRange pcr{};
    pcr.setStageFlags(vk::ShaderStageFlagBits::eCompute)
        .setOffset(0)
        .setSize(2 * sizeof(float));
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&*cc_->descriptor_set_layout())
        .setPushConstantRanges(pcr);
    const auto& params = cc_->grid_params();
    std::vector<vk::SpecializationMapEntry> mes = {
        {0, 0, sizeof(uint32_t)},   // constant_id 0 -> NX
        {1, sizeof(uint32_t), sizeof(uint32_t)}  // constant_id 1 -> NY
    };
    std::vector<uint8_t> bytes(2 * sizeof(uint32_t));
    memcpy(bytes.data(), &params.nx, sizeof(uint32_t));
    memcpy(bytes.data() + sizeof(uint32_t), &params.ny, sizeof(uint32_t));
    pipeline_layout_diffusion_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(ci));
    pipeline_layout_poisson_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(ci));
    pipeline_diffusion_ = dm_->CreateComputePipelineFromFile(
        *pipeline_layout_diffusion_, "shaders/diffusion.comp.spv", mes, bytes);
    pipeline_poisson_ = dm_->CreateComputePipelineFromFile(
        *pipeline_layout_poisson_, "shaders/poisson.comp.spv", mes, bytes);
}

void FluidSolvers::CreateVorticitySolverPipeline() {
    const auto& params = cc_->grid_params();
    std::vector<vk::SpecializationMapEntry> mes = {
        {0, 0, sizeof(uint32_t)},   // constant_id 0 -> NX
        {1, sizeof(uint32_t), sizeof(uint32_t)}  // constant_id 1 -> NY
    };
    std::vector<uint8_t> bytes(2 * sizeof(uint32_t));
    memcpy(bytes.data(), &params.nx, sizeof(uint32_t));
    memcpy(bytes.data() + sizeof(uint32_t), &params.ny, sizeof(uint32_t));

    vk::PushConstantRange push_range{};
    push_range.setStageFlags(vk::ShaderStageFlagBits::eCompute)
        .setOffset(0)
        .setSize(sizeof(float));

    vk::PipelineLayoutCreateInfo layout_ci{};
    layout_ci.setSetLayoutCount(1)
        .setPSetLayouts(&*cc_->descriptor_set_layout())
        .setPushConstantRangeCount(1)
        .setPPushConstantRanges(&push_range);

    pipeline_layout_vorticity_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(layout_ci));
    pipeline_vorticity_ = dm_->CreateComputePipelineFromFile(
        *pipeline_layout_vorticity_, "shaders/vorticity.comp.spv");
}

}  // namespace rtfs2d
