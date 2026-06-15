//
// Created by PC on 2026/6/11.
//

#ifndef RTFS2D_FLUID_SOLVERS_H
#define RTFS2D_FLUID_SOLVERS_H

#include <memory>

#include "compute_pipeline_factory.h"

namespace rtfs2d {

class DeviceManager;
class ComputeContext;

class FluidSolvers {
public:
    FluidSolvers(DeviceManager& dm, ComputeContext& cc, DescriptorSets& ds);

    ComputeShaderTask& task_advection() const { return *task_advection_; }
    ComputeShaderTask& task_divergence() const { return *task_divergence_; }
    ComputeShaderTask& task_diffusion() const { return *task_diffusion_; }
    ComputeShaderTask& task_poisson() const { return *task_poisson_; }
    ComputeShaderTask& task_projection() const { return *task_projection_; }
    ComputeShaderTask& task_vorticity() const { return *task_vorticity_; }
    ComputeShaderTask& task_ibm_interpolate() const { return *task_ibm_interpolate_; }
    ComputeShaderTask& task_ibm_spread() const { return *task_ibm_spread_; }
    ComputeShaderTask& task_ibm_apply_force() const { return *task_ibm_apply_force_; }
    ComputeShaderTask& task_ibm_mask() const { return *task_ibm_mask_; }
    ComputeShaderTask& task_compute_scalar() const { return *task_compute_scalar_; }
    ComputeShaderTask& task_smooth_velocity() const { return *task_smooth_velocity_; }
    ComputeShaderTask& task_dye_advection() const { return *task_dye_advection_; }
    ComputeShaderTask& task_dye_source() const { return *task_dye_source_; }

private:
    ComputeShaderTaskFactory factory_;
    std::unique_ptr<ComputeShaderTask> task_advection_;
    std::unique_ptr<ComputeShaderTask> task_divergence_;
    std::unique_ptr<ComputeShaderTask> task_diffusion_;
    std::unique_ptr<ComputeShaderTask> task_poisson_;
    std::unique_ptr<ComputeShaderTask> task_projection_;
    std::unique_ptr<ComputeShaderTask> task_vorticity_;
    std::unique_ptr<ComputeShaderTask> task_ibm_interpolate_;
    std::unique_ptr<ComputeShaderTask> task_ibm_spread_;
    std::unique_ptr<ComputeShaderTask> task_ibm_apply_force_;
    std::unique_ptr<ComputeShaderTask> task_ibm_mask_;
    std::unique_ptr<ComputeShaderTask> task_compute_scalar_;
    std::unique_ptr<ComputeShaderTask> task_smooth_velocity_;
    std::unique_ptr<ComputeShaderTask> task_dye_advection_;
    std::unique_ptr<ComputeShaderTask> task_dye_source_;
};

}

#endif //RTFS2D_FLUID_SOLVERS_H
