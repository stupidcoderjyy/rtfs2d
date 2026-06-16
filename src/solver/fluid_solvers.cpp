//
// Created by PC on 2026/6/11.
//

#include "fluid_solvers.h"

#include "case_data.h"
#include "vulkan/device_manager.h"

namespace rtfs2d {

FluidSolvers::FluidSolvers(DeviceManager &dm, ComputeContext& cc, const CaseData& case_data, DescriptorSets& ds): factory_(dm, cc, ds) {
    uint32_t nx = case_data.nx(), ny = case_data.ny();
    float inv_dx = 1.0f / case_data.dx(), inv_dy = 1.0f / case_data.dy();
    task_advection_ = factory_.Create("shaders/advection.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_divergence_ = factory_.Create("shaders/diverge.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .Build();
    task_projection_ = factory_.Create("shaders/project.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_diffusion_ = factory_.Create("shaders/diffusion.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(2 * sizeof(float))
        .Build();
    task_poisson_ = factory_.Create("shaders/poisson.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(3 * sizeof(float))
        .Build();
    task_vorticity_ = factory_.Create("shaders/vorticity.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_ibm_interpolate_ = factory_.Create("shaders/ibm_interpolate.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_ibm_spread_ = factory_.Create("shaders/ibm_spread_markers.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_ibm_apply_force_ = factory_.Create("shaders/ibm_apply_force.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_ibm_mask_ = factory_.Create("shaders/ibm_mask.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({case_data.dx(), case_data.dy()})
        .Build();
    task_compute_scalar_ = factory_.Create("shaders/compute_scalar.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(uint32_t) + sizeof(float))
        .Build();
    task_smooth_velocity_ = factory_.Create("shaders/smooth_velocity.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_dye_advection_ = factory_.Create("shaders/dye_advection.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({inv_dx, inv_dy})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_dye_source_ = factory_.Create("shaders/dye_source.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .AppendSpecializationConst<float>({case_data.dx(), case_data.dy()})
        .SetPushConstSize(sizeof(float) * 3)
        .Build();
}

}  // namespace