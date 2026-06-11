//
// Created by PC on 2026/6/11.
//

#include "vk_boundary.h"

#include <ranges>
#include "grid.h"
#include "vk_device.h"
#include "vk_memory.h"

using namespace rtfs2d;

BoundaryContext::BoundaryContext(DeviceManager& dm, const GridParams& gp) :
        dm_(&dm), grid_params_(gp) {
    top_count_ = grid_params_.nx;
    bottom_count_ = grid_params_.nx;
    left_count_ = grid_params_.ny;
    right_count_ = grid_params_.ny;
    bc_total_count_ = 2 * (grid_params_.nx + grid_params_.ny);

    size_t type_size = bc_total_count_ * sizeof(int32_t);
    auto [type_buf, type_mem] = AllocateBuffer(
        dm_->device(),
        dm_->physical_device(),
        type_size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    bc_type_ = std::move(type_buf);
    bc_type_memory_ = std::move(type_mem);

    size_t vel_size = bc_total_count_ * sizeof(float);
    auto [u_buf, u_mem] = AllocateBuffer(
        dm_->device(),
        dm_->physical_device(),
        vel_size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    bc_vel_u_ = std::move(u_buf);
    bc_vel_u_memory_ = std::move(u_mem);

    auto [v_buf, v_mem] = AllocateBuffer(
        dm_->device(),
        dm_->physical_device(),
        vel_size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    bc_vel_v_ = std::move(v_buf);
    bc_vel_v_memory_ = std::move(v_mem);

    std::vector init_type(bc_total_count_, static_cast<int32_t>(BoundaryType::kNone));
    std::vector init_vel(bc_total_count_, 0.0f);
    UploadBufferData(
        dm_->device(),
        dm_->physical_device(),
        dm_->command_pool(),
        dm_->graphics_queue(),
        init_type,
        *bc_type_);
    UploadBufferData(
        dm_->device(),
        dm_->physical_device(),
        dm_->command_pool(),
        dm_->graphics_queue(),
        init_vel,
        *bc_vel_u_);
    UploadBufferData(
        dm_->device(),
        dm_->physical_device(),
        dm_->command_pool(),
        dm_->graphics_queue(),
        init_vel,
        *bc_vel_v_);
}

void BoundaryContext::SetBoundary(
        BoundaryDirection dir,
        BoundaryType type,
        float begin,
        float end,
        float u,
        float v) {
    int idx = static_cast<int>(dir);
    segments_[idx].push_back({begin, end, type, u, v});
}

void BoundaryContext::Upload() const {
    std::vector type_data(bc_total_count_, static_cast<int32_t>(BoundaryType::kNone));
    std::vector u_data(bc_total_count_, 0.0f);
    std::vector v_data(bc_total_count_, 0.0f);

    uint32_t offset = 0;
    for (int dir = 0; dir < 4; ++dir) {
        uint32_t num_cells = 0;
        if (dir == static_cast<int>(BoundaryDirection::kTop) ||
                dir == static_cast<int>(BoundaryDirection::kBottom)) {
            num_cells = grid_params_.nx;
        } else {
            num_cells = grid_params_.ny;
        }

        for (uint32_t i = 0; i < num_cells; ++i) {
            float t = num_cells == 1 ? 0.5f :
                      static_cast<float>(i) / static_cast<float>(num_cells - 1);
            for (const auto& segs = segments_[dir];
                    const auto &[begin, end, type, u, v] : std::views::reverse(segs)) {
                if (constexpr float kEps = 1e-6f; t >= begin - kEps && t <= end + kEps) {
                    type_data[offset + i] = static_cast<int32_t>(type);
                    u_data[offset + i] = u;
                    v_data[offset + i] = v;
                    break;
                }
            }
        }
        offset += num_cells;
    }

    UploadBufferData(
        dm_->device(),
        dm_->physical_device(),
        dm_->command_pool(),
        dm_->graphics_queue(),
        type_data,
        *bc_type_);
    UploadBufferData(
        dm_->device(),
        dm_->physical_device(),
        dm_->command_pool(),
        dm_->graphics_queue(),
        u_data,
        *bc_vel_u_);
    UploadBufferData(
        dm_->device(),
        dm_->physical_device(),
        dm_->command_pool(),
        dm_->graphics_queue(),
        v_data,
        *bc_vel_v_);
}

uint32_t BoundaryContext::bc_offset(BoundaryDirection dir) const {
    switch (dir) {
        case BoundaryDirection::kTop:    return 0;
        case BoundaryDirection::kBottom: return top_count_;
        case BoundaryDirection::kLeft:   return top_count_ + bottom_count_;
        case BoundaryDirection::kRight:  return top_count_ + bottom_count_ + left_count_;
        default: return 0;
    }
}

uint32_t BoundaryContext::bc_count(BoundaryDirection dir) const {
    switch (dir) {
        case BoundaryDirection::kTop:    return top_count_;
        case BoundaryDirection::kBottom: return bottom_count_;
        case BoundaryDirection::kLeft:   return left_count_;
        case BoundaryDirection::kRight:  return right_count_;
        default: return 0;
    }
}