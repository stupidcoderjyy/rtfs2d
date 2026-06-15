//
// Created by PC on 2026/6/11.
//

#include "fluid_solvers.h"

#include "vk_device.h"
#include "vk_compute.h"

namespace rtfs2d {

FluidSolvers::FluidSolvers(DeviceManager &dm, ComputeContext &cc, DescriptorSets& ds): factory_(dm, cc, ds) {
    auto& params = cc.grid_params();
    uint32_t nx = params.nx, ny = params.ny;
    task_advection_ = factory_.Create("shaders/advection.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_divergence_ = factory_.Create("shaders/diverge.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_projection_ = factory_.Create("shaders/project.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_diffusion_ = factory_.Create("shaders/diffusion.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(2 * sizeof(float))
        .Build();
    task_poisson_ = factory_.Create("shaders/poisson.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(2 * sizeof(float))
        .Build();
    task_vorticity_ = factory_.Create("shaders/vorticity.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(sizeof(float))
        .Build();
    task_ibm_interpolate_ = factory_.Create("shaders/ibm_interpolate.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_ibm_spread_ = factory_.Create("shaders/ibm_spread_markers.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_ibm_apply_force_ = factory_.Create("shaders/ibm_apply_force.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_ibm_mask_ = factory_.Create("shaders/ibm_mask.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_compute_scalar_ = factory_.Create("shaders/compute_scalar.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(sizeof(uint32_t))
        .Build();
    task_smooth_velocity_ = factory_.Create("shaders/smooth_velocity.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_dye_advection_ = factory_.Create("shaders/dye_advection.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .Build();
    task_dye_source_ = factory_.Create("shaders/dye_source.comp.spv")
        .AppendSpecializationConst<uint32_t>({nx, ny})
        .SetPushConstSize(sizeof(uint32_t) + sizeof(float) * 2)
        .Build();
}

}  // namespace