//
// Created by PC on 2026/6/9.
//

#include "vk_compute.h"

#include <spdlog/spdlog.h>
#include <vector>

#include "vk_descriptor.h"
#include "vk_device.h"
#include "vk_memory.h"

using namespace rtfs2d;

ComputeContext::ComputeContext(DeviceManager& dm, const GridParams& params):
        dm_(&dm), grid_params_(params),
        compute_cell_count_(params.TotalCells()),
        compute_buf_size_(compute_cell_count_ * sizeof(float)) {
    boundary_ctx_ = std::make_unique<BoundaryContext>(dm, params);
    CreateVelocityBuffers();
    CreateDescriptorSets();
    CreatePipelineLayout();
    InitializeVortexField();
    fluid_solvers_ = std::make_unique<FluidSolvers>(dm, *this);
    boundary_ctx_->BeginSetBoundary();
    boundary_ctx_->SetBoundary(BoundaryDirection::kLeft, BoundaryType::kVelocity,
        0.495, 0.505, 5.0f);
    boundary_ctx_->SetBoundary(BoundaryDirection::kTop, BoundaryType::kPressure,
        0.4,0.6);
    boundary_ctx_->EndSetBoundary();
}

void ComputeContext::RecordAndSubmit(const vk::raii::Queue& queue) const {
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    auto& cb = cbs[0];
    // 录制模拟任务
    RecordFluidStepCommands(queue, cb);
    // 提交与等待
    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    queue.submit(si);
    queue.waitIdle();

    // std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void ComputeContext::AddBufferMemoryWriteReadBarrier(
        const vk::raii::CommandBuffer &cb, int buf) const {
    vk::BufferMemoryBarrier barrier{};
    barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
        .setBuffer(*BufferAt(buf))
        .setSize(compute_buf_size_)
        .setOffset(0)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader,
        {}, nullptr, {barrier}, nullptr);
}

void ComputeContext::CreateVelocityBuffers() {
    velocity_buffers_.resize(kBindingsSize);
    velocity_memories_.resize(kBindingsSize);
    std::vector host_data(compute_cell_count_, 0.0f);
    for (int i = 0; i < kBindingsSize; ++i) {
        auto [buf, mem] = AllocateBuffer(dm_->device(), dm_->physical_device(), compute_buf_size_,
            vk::BufferUsageFlagBits::eStorageBuffer
                | vk::BufferUsageFlagBits::eTransferSrc
                | vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
        velocity_buffers_[i] = std::move(buf);
        velocity_memories_[i] = std::move(mem);
        UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
            dm_->graphics_queue(), host_data, *velocity_buffers_[i]);
    }
}

void ComputeContext::CreateDescriptorSets() {
    DescriptorSetBuilder dsb(dm_->device());
    for (int i = 0; i < kBindingsSize; ++i) {
        if (i < 5) {
            dsb.AddStorageBufferBinding(i, vk::ShaderStageFlagBits::eCompute
                | vk::ShaderStageFlagBits::eFragment);
        } else {
            dsb.AddStorageBufferBinding(i, vk::ShaderStageFlagBits::eCompute);
        }
    }
    std::vector<std::vector<int>> sets_registry{
        {kSetAdvection,      0, 0 /* u_src */, 1, 1 /* v_src */, 2, 2 /* u_dst */, 3, 3 /* v_dst */},
        {kSetVorticity,      0, 0 /* u_src */, 1, 1/* v_src */},
        {kSetDiffusionEven,  0, 2 /* u_src */, 1, 3 /* v_src */, 2, 0 /* u_dst */, 3, 1 /* v_dst */},
        {kSetDiffusionOdd,   0, 0 /* u_src */, 1, 1 /* v_src */, 2, 2 /* u_dst */, 3, 3 /* v_dst */},
        {kSetDivergence,     0, 0 /* u_src */, 1, 1 /* v_src */, 2, 2 /*  div  */},
        {kSetPressureEven,   0, 0 /* u_src */, 1, 1 /* v_src */, 2, 2 /*  div  */, 3, 4 /*  pi   */, 4, 3 /*  po   */},
        {kSetPressureOdd,    0, 0 /* u_src */, 1, 1 /* v_src */, 2, 2 /*  div  */, 3, 3 /*  pi   */, 4, 4 /*  po   */},
        {kSetProjection,     0, 0 /* u_src */, 1, 1 /* v_src */, 2, 4 /*   p   */},
    };
    auto [ds_layout, ds_pool, ds_sets] = dsb.build(sets_registry.size());
    descriptor_set_layout_ = std::move(ds_layout);
    descriptor_pool_ = std::move(ds_pool);
    descriptor_sets_ = std::move(ds_sets);
    for (const auto& sr : sets_registry) {
        auto it = sr.begin();
        int set_idx = *it++;
        auto& set = descriptor_sets_[set_idx];
        while (it != sr.end()) {
            int binding = *it++;
            int buffer = *it++;
            dsb.WriteBuffer(*set, binding, *velocity_buffers_[buffer]);
            // spdlog::debug("set{}: buf{} bind to {}", set_idx, buffer, binding);
        }
        dsb.WriteBuffer(*set, 5, boundary_ctx_->BufferAt(BoundaryDirection::kLeft));
        dsb.WriteBuffer(*set, 6, boundary_ctx_->BufferAt(BoundaryDirection::kRight));
        dsb.WriteBuffer(*set, 7, boundary_ctx_->BufferAt(BoundaryDirection::kBottom));
        dsb.WriteBuffer(*set, 8, boundary_ctx_->BufferAt(BoundaryDirection::kTop));
    }
}

void ComputeContext::CreatePipelineLayout() {
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&**descriptor_set_layout_);
    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(ci));
}

void ComputeContext::RecordFluidStepCommands(const vk::raii::Queue& queue, const vk::raii::CommandBuffer& cb) const {
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});

    // 平流
    // [0(u), 1(v), 2, 3, 4]
    AddBufferMemoryWriteReadBarrier(cb, 0);
    AddBufferMemoryWriteReadBarrier(cb, 1);
    fluid_solvers_->SolveAdvection(cb, DescriptorSetAt(kSetAdvection));

    // 涡量约束
    // [0(u), 1(v), 2, 3, 4]
    AddBufferMemoryWriteReadBarrier(cb, 0);
    AddBufferMemoryWriteReadBarrier(cb, 1);
    fluid_solvers_->SolveVorticity(cb, DescriptorSetAt(kSetVorticity), 0.5f);

    // 扩散迭代（u 和 v 各做一次雅可比迭代）
    float viscosity = 0.0f;
    float dx = grid_params_.dx;
    float alpha = viscosity * 0.016f / (dx * dx);
    float beta = 4.0f + alpha;
    // 必须是奇数次迭代，否则无法把u、v换到前两个缓冲中
    for (int iter = 0; iter < 21; ++iter) {
        if (iter & 1) {
            // [0(u), 1(v), 2, 3, 4]
            AddBufferMemoryWriteReadBarrier(cb, 0);
            AddBufferMemoryWriteReadBarrier(cb, 1);
            fluid_solvers_->SolveDiffusion(cb, DescriptorSetAt(kSetDiffusionOdd), alpha, beta);
            // [0, 1, 2(u), 3(v), 4]
        } else {
            // [0, 1, 2(u), 3(v), 4]
            AddBufferMemoryWriteReadBarrier(cb, 2);
            AddBufferMemoryWriteReadBarrier(cb, 3);
            fluid_solvers_->SolveDiffusion(cb, DescriptorSetAt(kSetDiffusionEven), alpha, beta);
            // [0(u), 1(v), 2, 3, 4] -> 退出循环 | 进入另一个分支
        }
    }

    // 散度计算
    // [0(u), 1(v), 2(div), 3, 4]
    AddBufferMemoryWriteReadBarrier(cb, 0);
    AddBufferMemoryWriteReadBarrier(cb, 1);
    fluid_solvers_->SolveDivergence(cb, DescriptorSetAt(kSetDivergence));

    // 压力求解迭代（雅可比迭代解泊松方程）
    float alpha_p = -(dx * dx);
    for (int iter = 0; iter < 50; ++iter) {
        float beta_p = 4.0f;
        if (iter & 1) {
            //[0(u), 1(v), 2(div), 3(pi), 4(po)]
            AddBufferMemoryWriteReadBarrier(cb, 3);
            fluid_solvers_->SolvePoisson(cb, DescriptorSetAt(kSetPressureOdd), alpha_p, beta_p);
            //[0(u), 1(v), 2(div), 3, 4(pi)] -> 退出循环
        } else {
            //[0(u), 1(v), 2(div), 3(po), 4(pi)]
            AddBufferMemoryWriteReadBarrier(cb, 4);
            fluid_solvers_->SolvePoisson(cb, DescriptorSetAt(kSetPressureEven), alpha_p, beta_p);
            //[0(u), 1(v), 2(div), 3(pi), 4]
        }
    }

    // 压力投影
    // [0(u), 1(v), 2(div), 3, 4(p)]
    AddBufferMemoryWriteReadBarrier(cb, 4);
    fluid_solvers_->SolveProjection(cb, DescriptorSetAt(kSetProjection));
    // [0(u), 1(v), 2(div), 3, 4(p)]

    cb.end();
}

void ComputeContext::InitializeVortexField() const {
    std::vector u_data(compute_cell_count_, 0.0f);
    std::vector v_data(compute_cell_count_, 0.0f);
    /*
    const uint32_t nx = grid_params_.nx;
    const uint32_t ny = grid_params_.ny;

    // 2. 遍历每个网格单元
    for (uint32_t j = 0; j < ny; ++j)
    {
        for (uint32_t i = 0; i < nx; ++i)
        {
            uint32_t k = i + j * nx;   // 一维索引

            // 网格单元中心的归一化坐标
            float x = static_cast<float>(i) / static_cast<float>(nx);
            float y = static_cast<float>(j) / static_cast<float>(ny);

            // 涡旋中心 1：全局中心 (0.5, 0.5)
            float cx1 = 0.5f, cy1 = 0.5f;
            float dx1 = x - cx1;
            float dy1 = y - cy1;
            float r2_1 = dx1 * dx1 + dy1 * dy1;
            float w1 = std::exp(-r2_1 * 50.0f);   // 高斯衰减

            // 旋转速度方向：(-dy, dx) 逆时针
            u_data[k] += w1 * (-dy1);
            v_data[k] += w1 *  dx1;

            // 涡旋中心 2：左下方 (0.25, 0.25)
            float cx2 = 0.25f, cy2 = 0.25f;
            float dx2 = x - cx2;
            float dy2 = y - cy2;
            float r2_2 = dx2 * dx2 + dy2 * dy2;
            float w2 = std::exp(-r2_2 * 50.0f);

            u_data[k] += w2 * (-dy2);
            v_data[k] += w2 *  dx2;
        }
    }*/
    UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
        dm_->graphics_queue(), u_data, BufferAt(0));
    UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
        dm_->graphics_queue(), v_data, BufferAt(1));
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
    AddBufferMemoryWriteReadBarrier(cb, buffer);
    cb.end();
    vk::SubmitInfo si1{};
    si1.setCommandBuffers(*cb);
    queue.submit(si1);
    queue.waitIdle();

    DebugReadBackBuffer(BufferAt(buffer), compute_buf_size_, [&log_prefix, this](auto* p, auto) {
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
    AddBufferMemoryWriteReadBarrier(cb, buffer);
    cb.end();
    vk::SubmitInfo si1{};
    si1.setCommandBuffers(*cb);
    queue.submit(si1);
    queue.waitIdle();

    DebugReadBackBuffer(BufferAt(buffer), compute_buf_size_, [&log_prefix, &indexes, this](auto* p, auto) {
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

void ComputeContext::DebugReadBackBoundaryBuffer(BoundaryDirection d) const {
    DebugReadBackBuffer(boundary_ctx_->BufferAt(d), boundary_ctx_->BufferSize(d),[](void* p, auto size) {
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
