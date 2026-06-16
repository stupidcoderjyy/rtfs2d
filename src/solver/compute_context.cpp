//
// Created by PC on 2026/6/9.
//

#include "compute_context.h"

#include <spdlog/spdlog.h>
#include <vector>

#include "case_data.h"
#include "vulkan/descriptor_sets.h"
#include "vulkan/device_manager.h"
#include "vulkan/vk_memory.h"
#include "vulkan/buffers.h"

namespace rtfs2d {

ComputeContext::ComputeContext(DeviceManager& dm, DescriptorSets& ds, const CaseData& case_data):
        dm_(&dm), case_data_(&case_data), ds_(&ds),
        cell_count_(case_data.total_cells()),
        compute_buf_size_(cell_count_ * sizeof(float)),
        poisson_iter_n(std::clamp(
            static_cast<uint32_t>(std::ceil(50 * (0.00195f * case_data.dx()))),
            1u, 100u)) {
    fluid_solvers_ = std::make_unique<FluidSolvers>(dm, *this, case_data, ds);
}

void ComputeContext::UploadCaseData() {
    InitializeVortexField();
    case_data_->boundary().UploadData(*case_data_, *dm_);
    UploadObstacles();
}

void ComputeContext::RecordCommands(const vk::raii::CommandBuffer& cb) {
    // u, v, markers → markers
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    EnsureBufferReadyForCompute(cb, buffers::kBufIbmMarker);
    fluid_solvers_->task_ibm_interpolate()
        .Begin(cb, ds_->SetAt(kSetIbmInterpolate))
        .End(ibm_marker_count_);
    // markers → force
    EnsureBufferReadyForCompute(cb, buffers::kBufIbmMarker);
    fluid_solvers_->task_ibm_spread()
        .Begin(cb, ds_->SetAt(kSetIbmSpreadMarkers))
        .End(ibm_marker_count_);
    // force → u, v
    EnsureBufferReadyForCompute(cb, buffers::kBufIbmForce);
    fluid_solvers_->task_ibm_apply_force()
        .Begin(cb, ds_->SetAt(kSetIbmApplyForce))
        .End(cell_count_);

    // 平流
    // [0(u_src), 1(v_src), 2(u_dst), 3(v_dst), 4]
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    fluid_solvers_->task_advection()
        .Begin(cb, ds_->SetAt(kSetAdvection))
        .End(cell_count_);

    if (vis_mode_ == VisMode::kDye) {
        auto set_dye_adv = dye_use_set1_ ? kSetDyeAdvection1 : kSetDyeAdvection2;
        auto set_dye_src = dye_use_set1_ ? kSetDyeSource1 : kSetDyeSource2;
        dye_use_set1_ = !dye_use_set1_;
        // 染料平流
        EnsureBufferReadyForCompute(cb, buffers::kBufV2);
        EnsureBufferReadyForCompute(cb, buffers::kBufV3);
        fluid_solvers_->task_dye_advection()
            .Begin(cb, ds_->SetAt(set_dye_adv))
            .End(cell_count_);
        // 在 dye_dst 上添加染料
        if (dye_injecting_) {
            fluid_solvers_->task_dye_source()
                .Begin(cb, ds_->SetAt(set_dye_src))
                .PushConstant<float>({dye_x_, dye_y_, 0.03f})
                .End(cell_count_);
        }
    }

    // 涡量约束
    // [0, 1, 2(u), 3(v), 4]
    // fluid_solvers_->SolveVorticity(cb, DescriptorSetAt(kSetVorticity), 0.0f);

    // 扩散迭代（u 和 v 各做一次雅可比迭代）
    float viscosity = 0.8f;
    float dx = case_data_->dx();
    float alpha = viscosity * 0.016f / (dx * dx);
    float beta = 4.0f + alpha;
    // 必须是奇数次迭代，否则无法把u、v换到前两个缓冲中
    for (int iter = 0; iter < poisson_iter_n; ++iter) {
        if (iter & 1) {
            // [0(u), 1(v), 2, 3, 4]
            EnsureBufferReadyForCompute(cb, buffers::kBufV0);
            EnsureBufferReadyForCompute(cb, buffers::kBufV1);
            fluid_solvers_->task_diffusion()
                .Begin(cb, ds_->SetAt(kSetDiffusionOdd))
                .PushConstant<float>({alpha, beta})
                .End(cell_count_);
            // [0, 1, 2(u), 3(v), 4]
        } else {
            // [0, 1, 2(u), 3(v), 4]
            EnsureBufferReadyForCompute(cb, buffers::kBufV2);
            EnsureBufferReadyForCompute(cb, buffers::kBufV3);
            fluid_solvers_->task_diffusion()
                .Begin(cb, ds_->SetAt(kSetDiffusionEven))
                .PushConstant<float>({alpha, beta})
                .End(cell_count_);
            // [0(u), 1(v), 2, 3, 4] -> 退出循环 | 进入另一个分支
        }
    }

    // 散度计算
    // [0(u), 1(v), 2(div), 3, 4]
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    fluid_solvers_->task_divergence()
        .Begin(cb, ds_->SetAt(kSetDivergence))
        .End(cell_count_);

    // 压力求解迭代（雅可比迭代解泊松方程） u, v, div → p
    float alpha_p = -(dx * dx);
    for (int iter = 0; iter < 50; ++iter) {
        float beta_p = 4.0f;
        if (iter & 1) {
            //[0(u), 1(v), 2(div), 3(pi), 4(po)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV3);
            fluid_solvers_->task_poisson()
                .Begin(cb, ds_->SetAt(kSetPressureOdd))
                .PushConstant<float>({alpha_p, beta_p})
                .End(cell_count_);
            //[0(u), 1(v), 2(div), 3, 4(pi)] -> 退出循环
        } else {
            //[0(u), 1(v), 2(div), 3(po), 4(pi)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV4);
            fluid_solvers_->task_poisson()
                .Begin(cb, ds_->SetAt(kSetPressureEven))
                .PushConstant<float>({alpha_p, beta_p})
                .End(cell_count_);
            //[0(u), 1(v), 2(div), 3(pi), 4]
        }
    }

    // 压力投影 p → u,v
    // [0(u), 1(v), 2(div), 3, 4(p)]
    EnsureBufferReadyForCompute(cb, buffers::kBufV4);
    fluid_solvers_->task_projection()
        .Begin(cb, ds_->SetAt(kSetProjection))
        .End(cell_count_);
    // [0(u), 1(v), 2(div), 3, 4(p)]

    if (vis_mode_ == VisMode::kDye) {
        auto buf_dye_dst = dye_use_set1_ ? buffers::kBufV5 : buffers::kBufV6;
        EnsureBufferReady(vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eFragmentShader, cb, buf_dye_dst);
    } else {
        if (vis_field_ == VisField::kVorticity) {
            // 平滑速度场以消除涡量可视化中的锯齿
            // u_src, v_src → u_dst, v_dst
            // [0(u_src), 1(v_src), 2(scalar), 3(u_sm), 4, 5(v_sm)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV0);
            EnsureBufferReadyForCompute(cb, buffers::kBufV1);
            fluid_solvers_->task_smooth_velocity()
                .Begin(cb, ds_->SetAt(kSetSmoothVelocity))
                .End(cell_count_);
            // [0, 1, 2(scalar), 3(u_sm), 4, 5(v_sm)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV3);
            EnsureBufferReadyForCompute(cb, buffers::kBufV5);
            fluid_solvers_->task_compute_scalar()
                .Begin(cb, ds_->SetAt(kSetComputeScalarVI))
                .PushConstant<uint32_t>({static_cast<uint32_t>(vis_field_)})
                .End(cell_count_);
        } else {
            // [0(u), 1(v), 2(scalar), 3, 4(p)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV0);
            EnsureBufferReadyForCompute(cb, buffers::kBufV1);
            fluid_solvers_->task_compute_scalar()
                .Begin(cb, ds_->SetAt(kSetComputeScalarOthers))\
                .PushConstant<uint32_t>({static_cast<uint32_t>(vis_field_)})
                .End(cell_count_);
            EnsureBufferReady(vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eFragmentShader, cb, buffers::kBufV2);
        }
    }
}

void ComputeContext::EnsureBufferReady(
        vk::PipelineStageFlagBits src_stage,
        vk::PipelineStageFlagBits dst_stage,
        const vk::raii::CommandBuffer &cb, int buf) const {
    vk::BufferMemoryBarrier barrier{};
    barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
        .setBuffer(*dm_->BufferAt(buf))
        .setSize(dm_->BufferSize(buf))
        .setOffset(0)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(src_stage, dst_stage,
        {}, nullptr, {barrier}, nullptr);
}

void ComputeContext::InitializeVortexField() const {
    std::vector u_data(cell_count_, 0.0f);
    std::vector v_data(cell_count_, 0.0f);
    dm_->UploadDeviceBufferData(u_data, dm_->BufferAt(buffers::kBufV0));
    dm_->UploadDeviceBufferData(v_data, dm_->BufferAt(buffers::kBufV1));
}

void ComputeContext::UploadObstacles() {
    case_data_->geometry().GenerateIBMMarkers(case_data_->dx());
    dm_->InitBuffer(buffers::kBufIbmPolygon, case_data_->geometry().SerializePolygonSSBO());
    dm_->InitBuffer(buffers::kBufIbmMarker, case_data_->geometry().SerializeMarkerSSBO());
    ibm_marker_count_ = case_data_->geometry().MarksCount();

    // 预计算障碍掩码
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    auto& cb = cbs[0];
    auto queue = dm_->graphics_queue();
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    fluid_solvers_->task_ibm_mask()
        .Begin(cb, ds_->SetAt(kSetIbmMask)) //描述符集随便选一个即可
        .End(cell_count_);
    cb.end();
    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    queue.submit(si);
    queue.waitIdle();
    spdlog::info("Obstacles uploaded: {} markers", ibm_marker_count_);
}

void ComputeContext::DebugReadBackBuffer(const vk::raii::Buffer &buf,
        uint32_t size,
        const std::function<void(void* p, uint32_t len)> &handler) const {
    // 创建 staging buffer
    vk::BufferCreateInfo ci{};
    ci.setSize(size)
      .setUsage(vk::BufferUsageFlagBits::eTransferDst)
      .setSharingMode(vk::SharingMode::eExclusive);
    auto staging_buf = dm_->device().createBuffer(ci);

    auto mr = staging_buf.getMemoryRequirements();
    uint32_t mti = FindMemoryType(dm_->physical_device(), mr.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::MemoryAllocateInfo mai{};
    mai.setAllocationSize(mr.size).setMemoryTypeIndex(mti);
    auto staging_mem = dm_->device().allocateMemory(mai);
    staging_buf.bindMemory(*staging_mem, 0);

    // 分配一次性命令缓冲区
    vk::CommandBufferAllocateInfo cb_ai{};
    cb_ai.setCommandPool(dm_->command_pool())
         .setLevel(vk::CommandBufferLevel::ePrimary)
         .setCommandBufferCount(1);
    auto cbs = dm_->device().allocateCommandBuffers(cb_ai);
    auto& copy_cb = cbs[0];

    copy_cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::BufferCopy region{};
    region.setSize(size);
    copy_cb.copyBuffer(*buf, *staging_buf, region);
    copy_cb.end();

    vk::SubmitInfo si2{};
    si2.setCommandBuffers(*copy_cb);
    dm_->graphics_queue().submit(si2);
    dm_->graphics_queue().waitIdle();

    handler(staging_mem.mapMemory(0, size), size);
    staging_mem.unmapMemory();
}

void ComputeContext::DebugReadBackVelocityBuffer(
        const vk::raii::CommandBuffer& cb,
        int buffer,
        const std::string& log_prefix) const {
    // 提交并等待
    EnsureBufferReadyForCompute(cb, buffer);
    cb.end();
    vk::SubmitInfo si1{};
    si1.setCommandBuffers(*cb);
    auto& queue = dm_->graphics_queue();
    queue.submit(si1);
    queue.waitIdle();

    DebugReadBackBuffer(dm_->BufferAt(buffer), compute_buf_size_, [&log_prefix, this](auto* p, auto) {
        auto* ptr = static_cast<float*>(p);
        std::string msg = log_prefix + ": \n";
        for (int i = 0; i < case_data_->ny(); ++i) {
            for (int j = 0; j < case_data_->nx(); j++) {
                msg += std::to_string(*ptr++);
                msg += ", ";
            }
            msg += "\n";
        }
        spdlog::info(msg);
    });

    // 继续录制
    cb.reset();
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
}

void ComputeContext::DebugReadBackVelocityBufferPoints(
        const vk::raii::CommandBuffer &cb, int buffer,
        const std::string &log_prefix, const std::vector<int> &indexes) const {
    // 提交并等待
    EnsureBufferReadyForCompute(cb, buffer);
    cb.end();
    vk::SubmitInfo si1{};
    si1.setCommandBuffers(*cb);
    auto& queue = dm_->graphics_queue();
    queue.submit(si1);
    queue.waitIdle();

    DebugReadBackBuffer(dm_->BufferAt(buffer), compute_buf_size_, [&log_prefix, &indexes, this](auto* p, auto) {
        auto* ptr = static_cast<float*>(p);
        std::string msg = log_prefix + ": ";
        for (int idx : indexes) {
            if (idx >= 0 && idx < static_cast<int>(cell_count_)) {
                msg += "[" + std::to_string(idx) + "]=" + std::to_string(ptr[idx]) + " ";
            } else {
                msg += "[" + std::to_string(idx) + "]=out_of_range ";
            }
        }
        spdlog::info(msg);
    });

    // 继续录制
    cb.reset();
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
}

void ComputeContext::DebugReadBackBoundaryBuffer(int buffer) const {
    DebugReadBackBuffer(dm_->BufferAt(buffer), dm_->BufferSize(buffer),[](void* p, auto size) {
        auto* pUint = static_cast<uint32_t*>(p);
        size /= sizeof(uint32_t);
        std::string msg = "\n";
        int i = 0;
        while (i < size) {
            msg += std::to_string(pUint[i++]) + " ";
            msg += std::to_string(static_cast<float>(pUint[i++])) + " ";
            msg += std::to_string(static_cast<float>(pUint[i++])) + "\n";
        }
        spdlog::debug(msg);
    });
}

}  // namespace rtfs2d
