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

ComputeContext::ComputeContext(DeviceManager& dm): dm_(&dm) {
    boundary_ctx_ = std::make_unique<BoundaryContext>(dm, grid_params_);
    CreateVelocityBuffers();
    CreateDescriptorSets();
    CreatePipelineLayout();
    InitializeVortexField();
    advection_solver_ = std::make_unique<AdvectionSolver>(dm, *this);
    jacobi_solver_ = std::make_unique<JacobiSolver>(dm, *this);
    divergence_solver_ = std::make_unique<DivergenceSolver>(dm, *this);
    projection_solver_ = std::make_unique<ProjectionSolver>(dm, *this);
    vorticity_solver_ = std::make_unique<VorticitySolver>(dm, *this);
    pressure_bc_solver_ = std::make_unique<PressureBCSolver>(dm, *this);
    boundary_ctx_->SetBoundary(BoundaryDirection::kTop, BoundaryType::kNoSlipWall,0,0.2);
    boundary_ctx_->SetBoundary(BoundaryDirection::kTop, BoundaryType::kVelocityInlet, 0, 1, 0, -0.2f);
    boundary_ctx_->SetBoundary(BoundaryDirection::kBottom, BoundaryType::kNoSlipWall);
    boundary_ctx_->SetBoundary(BoundaryDirection::kBottom, BoundaryType::kPressureOutlet, 0.2, 0.4);
    boundary_ctx_->SetBoundary(BoundaryDirection::kBottom, BoundaryType::kPressureOutlet, 0.6, 0.8);;
    boundary_ctx_->SetBoundary(BoundaryDirection::kLeft, BoundaryType::kPressureOutlet);
    boundary_ctx_->SetBoundary(BoundaryDirection::kRight, BoundaryType::kPressureOutlet);
    boundary_ctx_->Upload();
}

void ComputeContext::RecordAndSubmit(const vk::raii::Queue& queue) const {
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(dm_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = dm_->device().allocateCommandBuffers(ai);
    auto& cb = cbs[0];
    // 录制模拟任务
    RecordFluidStepCommands(cb);
    // 提交与等待
    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    queue.submit(si);
    queue.waitIdle();

    //调试
    //DebugReadBack(2, "u", {5});
    //DebugReadBack(0, "v", {5});

    // 调试回读
    vk::BufferCreateInfo staging_ci{};
    staging_ci.setSize(compute_buf_size_)
              .setUsage(vk::BufferUsageFlagBits::eTransferDst)
              .setSharingMode(vk::SharingMode::eExclusive);
    auto staging_buf = dm_->device().createBuffer(staging_ci);

    auto mr = staging_buf.getMemoryRequirements();
    uint32_t mti = FindMemoryType(dm_->physical_device(), mr.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::MemoryAllocateInfo m_ai{};
    m_ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    auto staging_mem = dm_->device().allocateMemory(m_ai);
    staging_buf.bindMemory(*staging_mem, 0);
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
        {}, nullptr, barrier, nullptr);
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
    auto [ds_layout, ds_pool, ds_sets] = dsb.build(12);
    descriptor_set_layout_ = std::move(ds_layout);
    descriptor_pool_ = std::move(ds_pool);
    descriptor_sets_ = std::move(ds_sets);
    std::vector<std::vector<int>> sets_registry{
        {kSetAdvectionU,     0, 0 /* u_src */, 1, 2 /*  out  */, 2, 0 /* u_src */, 3, 1 /* v_src */},
        {kSetAdvectionV,     0, 1 /* v_src */, 1, 3 /*  out  */, 2, 2 /* u_src */, 3, 1 /* v_src */},
        {kSetDiffusionUEven, 0, 2 /* u_src */, 1, 2 /* u_src */, 3, 0 /*  out  */},
        {kSetDiffusionUOdd,  0, 0 /* u_src */, 1, 0 /* u_src */, 3, 2 /*  out  */},
        {kSetDiffusionVEven, 0, 3 /* v_src */, 1, 3 /* v_src */, 3, 1 /*  out  */},
        {kSetDiffusionVOdd,  0, 1 /* v_src */, 1, 1 /* v_src */, 3, 3 /*  out  */},
        {kSetDivergence,     1, 2 /*  div  */, 2, 0 /* u_src */, 3, 1 /* v_src */},
        {kSetPressureEven,   0, 4 /*  pi   */, 1, 2 /*  div  */, 3, 3 /*  po   */},
        {kSetPressureOdd,    0, 3 /*  pi   */, 1, 2 /*  div  */, 3, 4 /*  po   */},
        {kSetProjection,     0, 4 /*  p    */, 2, 0 /* u_src */, 3, 1 /* v_src */},
        {kSetVorticity,      2, 0 /* u_src */, 3, 1 /* v_src */},
        {kSetPressureBc,     0, 4 /*  p    */}
    };
    for (const auto& sr : sets_registry) {
        auto it = sr.begin();
        int set_idx = *it++;
        auto& set = descriptor_sets_[set_idx];
        while (it != sr.end()) {
            int binding = *it++;
            int buffer = *it++;
            dsb.WriteBuffer(*set, binding, *velocity_buffers_[buffer]);
            // 边界条件
            if (set_idx == kSetPressureBc) {
                dsb.WriteBuffer(*set, 5, boundary_ctx_->bc_type_buffer());
            } else {
                dsb.WriteBuffer(*set, 5, boundary_ctx_->bc_type_buffer());
                dsb.WriteBuffer(*set, 6, boundary_ctx_->bc_vel_u_buffer());
                dsb.WriteBuffer(*set, 7, boundary_ctx_->bc_vel_v_buffer());
            }
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
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});

    // 平流u分量
    //[0(u), 1(v), 2(o), 3, 4]
    advection_solver_->RecordCommands(cb, DescriptorSetAt(kSetAdvectionU));
    AddBufferMemoryWriteReadBarrier(cb, 2);
    //[0, 1(v), 2(u), 3, 4]

    // 平流v分量
    //[0, 1(v), 2(u), 3(o), 4]
    advection_solver_->RecordCommands(cb, DescriptorSetAt(kSetAdvectionV));
    AddBufferMemoryWriteReadBarrier(cb, 0);
    //[0, 1, 2(u), 3(v), 4]

    // 涡量约束
    //[0(u), 1(v), 2, 3, 4]
    vorticity_solver_->RecordCommands(cb, DescriptorSetAt(kSetVorticity), .4f);

    // 扩散迭代（u 和 v 各做一次雅可比迭代）
    float viscosity = 0.0f;
    float dx = grid_params_.dx;
    float alpha = viscosity * 0.016f / (dx * dx);
    float beta = 4.0f + alpha;
    // 必须是奇数次迭代，否则无法把u、v换到前两个缓冲中
    for (int iter = 0; iter < 13; ++iter) {
        if (iter & 1) {
            //[0(u), 1(v), 2(o), 3, 4]
            jacobi_solver_->RecordCommands(cb, DescriptorSetAt(kSetDiffusionUOdd), alpha, beta);
            AddBufferMemoryWriteReadBarrier(cb, 2);
            //[0, 1(v), 2(u), 3(o), 4]
            jacobi_solver_->RecordCommands(cb, DescriptorSetAt(kSetDiffusionVOdd), alpha, beta);
            AddBufferMemoryWriteReadBarrier(cb, 0);
            //[0, 1, 2(u), 3(v), 4]
        } else {
            //[0(o), 1, 2(u), 3(v), 4]
            jacobi_solver_->RecordCommands(cb, DescriptorSetAt(kSetDiffusionUEven), alpha, beta);
            AddBufferMemoryWriteReadBarrier(cb, 1);
            //[0(u), 1(o), 2, 3(v), 4]
            jacobi_solver_->RecordCommands(cb, DescriptorSetAt(kSetDiffusionVEven), alpha, beta);
            AddBufferMemoryWriteReadBarrier(cb, 3);
            //[0(u), 1(v), 2, 3, 4] -> 退出循环 | 进入另一个分支
        }
    }


    // 散度计算
    //[0(u), 1(v), 2(div), 3, 4]
    divergence_solver_->RecordCommands(cb, DescriptorSetAt(kSetDivergence));
    AddBufferMemoryWriteReadBarrier(cb, 1);
    // 压力求解迭代（雅可比迭代解泊松方程）
    float alpha_p = -(dx * dx);
    for (int iter = 0; iter < 50; ++iter) {
        float beta_p = 4.0f;
        if (iter & 1) {
            //[0(u), 1(v), 2(div), 3(pi), 4(po)]
            jacobi_solver_->RecordCommands(cb, DescriptorSetAt(kSetPressureOdd), alpha_p, beta_p);
            AddBufferMemoryWriteReadBarrier(cb, 4);
            //[0(u), 1(v), 2(div), 3, 4(pi)] -> 退出循环
        } else {
            //[0(u), 1(v), 2(div), 3(po), 4(pi)]
            jacobi_solver_->RecordCommands(cb, DescriptorSetAt(kSetPressureEven), alpha_p, beta_p);
            AddBufferMemoryWriteReadBarrier(cb, 3);
            //[0(u), 1(v), 2(div), 3(pi), 4]
        }
    }

    // 清除压力边界的压力值
    //[0, 1, 2, 3, 4(p)]
    // pressure_bc_solver_->RecordCommands(cb, DescriptorSetAt(kSetPressureBc));
    // AddBufferMemoryWriteReadBarrier(cb, 4);

    // 压力投影
    //[0(u), 1(v), 2(div), 3, 4(p)]
    projection_solver_->RecordCommands(cb, DescriptorSetAt(kSetProjection));
    //[0(u), 1(v), 2(div), 3, 4(p)]
    AddBufferMemoryWriteReadBarrier(cb, 0);
    AddBufferMemoryWriteReadBarrier(cb, 1);

    cb.end();
}
void ComputeContext::InitializeVortexField() const {
    // 1. 构造 u/v 数据容器，初始化为 0.0f
    std::vector u_data(compute_cell_count_, 0.0f);
    std::vector v_data(compute_cell_count_, 0.0f);
    //
    // const uint32_t nx = grid_params_.nx;
    // const uint32_t ny = grid_params_.ny;
    //
    // // 2. 遍历每个网格单元
    // for (uint32_t j = 0; j < ny; ++j)
    // {
    //     for (uint32_t i = 0; i < nx; ++i)
    //     {
    //         uint32_t k = i + j * nx;   // 一维索引
    //
    //         // 网格单元中心的归一化坐标
    //         float x = static_cast<float>(i) / static_cast<float>(nx);
    //         float y = static_cast<float>(j) / static_cast<float>(ny);
    //
    //         // 涡旋中心 1：全局中心 (0.5, 0.5)
    //         float cx1 = 0.5f, cy1 = 0.5f;
    //         float dx1 = x - cx1;
    //         float dy1 = y - cy1;
    //         float r2_1 = dx1 * dx1 + dy1 * dy1;
    //         float w1 = std::exp(-r2_1 * 50.0f);   // 高斯衰减
    //
    //         // 旋转速度方向：(-dy, dx) 逆时针
    //         u_data[k] += w1 * (-dy1);
    //         v_data[k] += w1 *  dx1;
    //
    //         // 涡旋中心 2：左下方 (0.25, 0.25)
    //         float cx2 = 0.25f, cy2 = 0.25f;
    //         float dx2 = x - cx2;
    //         float dy2 = y - cy2;
    //         float r2_2 = dx2 * dx2 + dy2 * dy2;
    //         float w2 = std::exp(-r2_2 * 50.0f);
    //
    //         u_data[k] += w2 * (-dy2);
    //         v_data[k] += w2 *  dx2;
    //     }
    // }

    // 3. 上传到 GPU 缓冲区（假设 eUSrc 和 eVSrc 是速度缓冲区的枚举标识）
    UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
        dm_->graphics_queue(), u_data, BufferAt(0));
    UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
        dm_->graphics_queue(), v_data, BufferAt(1));
}

void ComputeContext::DebugReadBack(int buffer, const std::string& log_prefix, const std::vector<int> &indexes) const {
    // 创建 staging buffer
    vk::BufferCreateInfo ci{};
    ci.setSize(compute_buf_size_)
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
    region.setSize(compute_buf_size_);
    copy_cb.copyBuffer(*BufferAt(buffer), *staging_buf, region);
    copy_cb.end();

    vk::SubmitInfo si{};
    si.setCommandBuffers(*copy_cb);
    dm_->graphics_queue().submit(si);
    dm_->graphics_queue().waitIdle();

    auto* ptr = static_cast<float*>(staging_mem.mapMemory(0, compute_buf_size_));
    std::string msg = log_prefix + ": ";
    for (int idx : indexes) {
        if (idx >= 0 && idx < static_cast<int>(compute_cell_count_)) {
            msg += "[" + std::to_string(idx) + "]=" + std::to_string(ptr[idx]) + " ";
        } else {
            msg += "[" + std::to_string(idx) + "]=out_of_range ";
        }
    }
    spdlog::info(msg);
    staging_mem.unmapMemory();
}
