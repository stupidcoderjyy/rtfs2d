//
// Created by PC on 2026/6/9.
//

#include "vk_compute.h"

#include <spdlog/spdlog.h>
#include <vector>

#include "vk_descriptor.h"
#include "vk_device.h"
#include "vk_memory.h"

namespace rtfs2d {

ComputeContext::ComputeContext(DeviceManager& dm, const GridParams& params):
        dm_(&dm), grid_params_(params),
        compute_cell_count_(params.TotalCells()),
        compute_buf_size_(compute_cell_count_ * sizeof(float)) {
    boundary_ctx_ = std::make_unique<BoundaryContext>(dm, params);
    CreateBuffers();
    CreateDescriptorSets();
    CreatePipelineLayout();
    InitializeVortexField();
    fluid_solvers_ = std::make_unique<FluidSolvers>(dm, *this);
    boundary_ctx_->BeginSetBoundary();
    boundary_ctx_->SetBoundary(BoundaryDirection::kLeft, BoundaryType::kVelocity,
        0, 1, 0.7f);
    boundary_ctx_->SetBoundary(BoundaryDirection::kRight, BoundaryType::kPressure,
        0,1);
    boundary_ctx_->EndSetBoundary();
    AddDebugGeometry();
}

void ComputeContext::RecordAndSubmit(const vk::raii::CommandBuffer& cb) const {
    cb.reset();
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    RecordFluidStepCommands(cb);
    cb.end();
    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    dm_->graphics_queue().submit(si);
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

void ComputeContext::CreateBuffers() const {
    std::vector host_data(compute_cell_count_, 0.0f);
    auto usage = vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eTransferSrc
        | vk::BufferUsageFlagBits::eTransferDst;
    auto mem_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    for (int i = buffers::kBufV0; i <= buffers::kBufV4; ++i) {
        dm_->CreateBuffer(i, compute_buf_size_, usage, mem_flags);
        dm_->InitBuffer(i, host_data);
    }
    // 创建多边形 SSBO
    dm_->CreateBuffer(buffers::kBufIbmPolygon, ObstacleGeometry::kPolygonBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    dm_->CreateBuffer(buffers::kBufIbmMarker, ObstacleGeometry::kMarkBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    dm_->CreateBuffer(buffers::kBufIbmForce, 2 * compute_cell_count_ * sizeof(float),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    dm_->CreateBuffer(buffers::kBufIbmMask, 2 * compute_cell_count_ * sizeof(float),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    // 初始化为零
    dm_->InitBuffer<uint8_t>(buffers::kBufIbmPolygon, 0);
    dm_->InitBuffer<uint8_t>(buffers::kBufIbmMarker, 0);
    dm_->InitBuffer<uint8_t>(buffers::kBufIbmForce, 0);
    dm_->InitBuffer<uint8_t>(buffers::kBufIbmMask, 0);
}

void ComputeContext::CreateDescriptorSets() {
    DescriptorSetBuilder dsb(dm_->device());
    for (int i = 0; i < 5; ++i) {
        dsb.AddStorageBufferBinding(i,
            vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment);
    }
    for (int i = 5; i < 9; ++i) {
        dsb.AddStorageBufferBinding(i, vk::ShaderStageFlagBits::eCompute);
    }
    for (int i = 9; i < 11; ++i) {
        dsb.AddStorageBufferBinding(i,
            vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment);
    }
    dsb.AddStorageBufferBinding(11, vk::ShaderStageFlagBits::eCompute);
    dsb.AddStorageBufferBinding(12, vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment);

    std::vector<std::vector<int>> field_reg{
        {kSetAdvection,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /* u_dst */,
            3, buffers::kBufV3 /* v_dst */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetVorticity,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetDiffusionEven,
            0, buffers::kBufV2 /* u_src */,
            1, buffers::kBufV3 /* v_src */,
            2, buffers::kBufV0 /* u_dst */,
            3, buffers::kBufV1 /* v_dst */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetDiffusionOdd,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /* u_dst */,
            3, buffers::kBufV3 /* v_dst */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetDivergence,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /*  div  */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetPressureEven,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /*  div  */,
            3, buffers::kBufV4 /*  pi   */,
            4, buffers::kBufV3 /*  po   */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetPressureOdd,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /*  div  */,
            3, buffers::kBufV3 /*  pi   */,
            4, buffers::kBufV4 /*  po   */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetProjection,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV4 /*   p   */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        }, {kSetIbmApplyForce,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            11, buffers::kBufIbmForce,
            12, buffers::kBufIbmMask,
        }, {kSetIbmInterpolate,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            10, buffers::kBufIbmMarker,
        }, {kSetIbmMask,
            9, buffers::kBufIbmPolygon,
            12, buffers::kBufIbmMask,
        }, {kSetIbmSpreadMarkers,
            10, buffers::kBufIbmMarker,
            11, buffers::kBufIbmForce,
        }, {kSetComputeScalar,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV3 /* scalar*/,
            3, buffers::kBufV4 /*   p   */,
        }, {kSetVisualization,
            2, buffers::kBufV3 /* scalar*/,
            12, buffers::kBufIbmMask,
        }
    };
    auto [ds_layout, ds_pool, ds_sets] = dsb.build(field_reg.size());
    descriptor_set_layout_ = std::move(ds_layout);
    descriptor_pool_ = std::move(ds_pool);
    descriptor_sets_ = std::move(ds_sets);
    for (const auto& sr : field_reg) {
        auto it = sr.begin();
        int set_idx = *it++;
        auto& set = descriptor_sets_[set_idx];
        while (it != sr.end()) {
            int binding = *it++;
            int buffer = *it++;
            dsb.WriteBuffer(*set, binding, dm_->BufferAt(buffer));
        }
    }
}

void ComputeContext::CreatePipelineLayout() {
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&**descriptor_set_layout_);
    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(ci));
}

void ComputeContext::RecordFluidStepCommands(const vk::raii::CommandBuffer& cb) const {
    // u, v, markers → markers
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    EnsureBufferReadyForCompute(cb, buffers::kBufIbmMarker);
    fluid_solvers_->SolveIBMInterpolate(cb, DescriptorSetAt(kSetIbmInterpolate));
    // markers → force
    EnsureBufferReadyForCompute(cb, buffers::kBufIbmMarker);
    fluid_solvers_->SolveIBMSpread(cb, DescriptorSetAt(kSetIbmSpreadMarkers));
    // force → u, v
    EnsureBufferReadyForCompute(cb, buffers::kBufIbmForce);
    fluid_solvers_->SolveIBMApplyForce(cb, DescriptorSetAt(kSetIbmApplyForce));

    // 平流
    // [0(u), 1(v), 2, 3, 4]
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    fluid_solvers_->SolveAdvection(cb, DescriptorSetAt(kSetAdvection));

    // 涡量约束
    // [0(u), 1(v), 2, 3, 4]
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    fluid_solvers_->SolveVorticity(cb, DescriptorSetAt(kSetVorticity), 0.5f);

    // 扩散迭代（u 和 v 各做一次雅可比迭代）
    float viscosity = 0.8f;
    float dx = grid_params_.dx;
    float alpha = viscosity * 0.016f / (dx * dx);
    float beta = 4.0f + alpha;
    // 必须是奇数次迭代，否则无法把u、v换到前两个缓冲中
    for (int iter = 0; iter < 21; ++iter) {
        if (iter & 1) {
            // [0(u), 1(v), 2, 3, 4]
            EnsureBufferReadyForCompute(cb, buffers::kBufV0);
            EnsureBufferReadyForCompute(cb, buffers::kBufV1);
            fluid_solvers_->SolveDiffusion(cb, DescriptorSetAt(kSetDiffusionOdd), alpha, beta);
            // [0, 1, 2(u), 3(v), 4]
        } else {
            // [0, 1, 2(u), 3(v), 4]
            EnsureBufferReadyForCompute(cb, buffers::kBufV2);
            EnsureBufferReadyForCompute(cb, buffers::kBufV3);
            fluid_solvers_->SolveDiffusion(cb, DescriptorSetAt(kSetDiffusionEven), alpha, beta);
            // [0(u), 1(v), 2, 3, 4] -> 退出循环 | 进入另一个分支
        }
    }

    // 散度计算
    // [0(u), 1(v), 2(div), 3, 4]
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    fluid_solvers_->SolveDivergence(cb, DescriptorSetAt(kSetDivergence));

    // 压力求解迭代（雅可比迭代解泊松方程） u, v, div → p
    float alpha_p = -(dx * dx);
    for (int iter = 0; iter < 50; ++iter) {
        float beta_p = 4.0f;
        if (iter & 1) {
            //[0(u), 1(v), 2(div), 3(pi), 4(po)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV3);
            fluid_solvers_->SolvePoisson(cb, DescriptorSetAt(kSetPressureOdd), alpha_p, beta_p);
            //[0(u), 1(v), 2(div), 3, 4(pi)] -> 退出循环
        } else {
            //[0(u), 1(v), 2(div), 3(po), 4(pi)]
            EnsureBufferReadyForCompute(cb, buffers::kBufV4);
            fluid_solvers_->SolvePoisson(cb, DescriptorSetAt(kSetPressureEven), alpha_p, beta_p);
            //[0(u), 1(v), 2(div), 3(pi), 4]
        }
    }

    // 压力投影 p → u,v
    // [0(u), 1(v), 2(div), 3, 4(p)]
    EnsureBufferReadyForCompute(cb, buffers::kBufV4);
    fluid_solvers_->SolveProjection(cb, DescriptorSetAt(kSetProjection));
    // [0(u), 1(v), 2(div), 3, 4(p)]

    // 计算用于可视化的标量场 u, v, p → scalar
    // [0(u), 1(v), 2, 3(scalar), 4(p)]
    EnsureBufferReadyForCompute(cb, buffers::kBufV0);
    EnsureBufferReadyForCompute(cb, buffers::kBufV1);
    fluid_solvers_->ComputeScalar(cb, DescriptorSetAt(kSetComputeScalar));
    EnsureBufferReady(vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader, cb, buffers::kBufV3);
}

void ComputeContext::InitializeVortexField() const {
    std::vector u_data(compute_cell_count_, 0.0f);
    std::vector v_data(compute_cell_count_, 0.0f);
    dm_->UploadDeviceBufferData(u_data, dm_->BufferAt(buffers::kBufV0));
    dm_->UploadDeviceBufferData(v_data, dm_->BufferAt(buffers::kBufV1));
}

void ComputeContext::UploadObstacles(const ObstacleGeometry &geom) {
    dm_->InitBuffer(buffers::kBufIbmPolygon, geom.SerializePolygonSSBO());
    dm_->InitBuffer(buffers::kBufIbmMarker, geom.SerializeMarkerSSBO());
    ibm_marker_count_ = geom.MarksCount();

    // 预计算障碍掩码
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    auto& cb = cbs[0];
    auto queue = dm_->graphics_queue();
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    fluid_solvers_->PrecomputeIBMMask(cb, DescriptorSetAt(kSetIbmMask)); //描述符集随便选一个即可
    cb.end();
    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    queue.submit(si);
    queue.waitIdle();
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
    cb_ai.setCommandPool(dm_->compute_command_pool())
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
        const vk::raii::Queue& queue,
        const vk::raii::CommandBuffer& cb,
        int buffer,
        const std::string& log_prefix) const {
    // 提交并等待
    EnsureBufferReadyForCompute(cb, buffer);
    cb.end();
    vk::SubmitInfo si1{};
    si1.setCommandBuffers(*cb);
    queue.submit(si1);
    queue.waitIdle();

    DebugReadBackBuffer(dm_->BufferAt(buffer), compute_buf_size_, [&log_prefix, this](auto* p, auto) {
        auto* ptr = static_cast<float*>(p);
        std::string msg = log_prefix + ": \n";
        for (int i = 0; i < grid_params_.ny; ++i) {
            for (int j = 0; j < grid_params_.nx; j++) {
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

void ComputeContext::DebugReadBackVelocityBufferPoints(const vk::raii::Queue &queue,
        const vk::raii::CommandBuffer &cb, int buffer,
        const std::string &log_prefix, const std::vector<int> &indexes) const {

    // 提交并等待
    EnsureBufferReadyForCompute(cb, buffer);
    cb.end();
    vk::SubmitInfo si1{};
    si1.setCommandBuffers(*cb);
    queue.submit(si1);
    queue.waitIdle();

    DebugReadBackBuffer(dm_->BufferAt(buffer), compute_buf_size_, [&log_prefix, &indexes, this](auto* p, auto) {
        auto* ptr = static_cast<float*>(p);
        std::string msg = log_prefix + ": ";
        for (int idx : indexes) {
            if (idx >= 0 && idx < static_cast<int>(compute_cell_count_)) {
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

void ComputeContext::AddDebugGeometry() {
    ObstacleGeometry geom;
    std::vector<std::array<float,2>> points;
    for (float rad = 0; rad < 2 * M_PI; rad += M_PI / 16.0f) {
        float r =  0.05f;
        float y0 = 0.5f;
        float x0 = 0.1f;
        points.push_back({
            x0 + r * std::cos(rad),
            y0 + r * std::sin(rad)
        });
    }
    geom.AddObstacle(points);
    geom.GenerateIBMMarkers(grid_params_.dx);
    UploadObstacles(geom);
}

}  // namespace rtfs2d
