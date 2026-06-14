//
// Created by PC on 2026/6/11.
//

#ifndef RTFS2D_FLUID_SOLVERS_H
#define RTFS2D_FLUID_SOLVERS_H

#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class DeviceManager;
class ComputeContext;

class FluidSolvers {
public:
    FluidSolvers(DeviceManager& dm, ComputeContext& cc);

    void SolveAdvection(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void SolveDivergence(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void SolveDiffusion(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds,
        float alpha, float beta) const;
    void SolvePoisson(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds,
        float alpha, float beta) const;
    void SolveProjection(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void SolveVorticity(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds,
        float epsilon) const;
    void SolveIBMInterpolate(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void SolveIBMSpread(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void SolveIBMApplyForce(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void PrecomputeIBMMask(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;
    void ComputeScalar(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet &ds) const;
private:
    static constexpr int kWorkGroupSize = 128;
    DeviceManager* dm_;
    ComputeContext* cc_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_advection_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_divergence_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_diffusion_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_diffusion_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_poisson_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_poisson_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_projection_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_vorticity_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_vorticity_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_ibm_interpolate_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_ibm_spread_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_ibm_apply_force_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_ibm_mask_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_compute_scalar_;

    std::unique_ptr<vk::raii::Pipeline> CreateSolverPipeline(const std::string& shader) const;
    void CreateJacobiPipelines();
    void CreateVorticitySolverPipeline();
};

}

#endif //RTFS2D_FLUID_SOLVERS_H
