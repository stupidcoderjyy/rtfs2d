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
    h_cell_count_ = grid_params_.nx - 2;
    v_cell_count_ = grid_params_.ny - 2;
    h_buf_size_ = h_cell_count_ * kBufferUnitSize;
    v_buf_size_ = v_cell_count_ * kBufferUnitSize;
    for (int i = 0; i < 4; ++i) {
        auto buf_size = i <= 1 ? v_buf_size_ : h_buf_size_;
        auto [type_buf, type_mem] = AllocateBuffer(
            dm_->device(),
            dm_->physical_device(),
            buf_size,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        //默认边界条件：无滑移墙壁
        std::vector<uint8_t> initial(buf_size, 0); // kNoSlipWall + 0.0f + 0.0f
        UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
            dm_->graphics_queue(), initial, *type_buf);
        buffer_bc_info_[i] = std::move(type_buf);
        memory_bc_info_[i] = std::move(type_mem);
    }
}

void BoundaryContext::SetBoundary(BoundaryDirection dir, BoundaryType type,
        float begin, float end, float u, float v) {
    int idx = static_cast<int>(dir);
    segments_[idx].push_back({begin, end, type, u, v});
}

void BoundaryContext::BeginSetBoundary() {
    for (auto& seg_vec : segments_) {
        seg_vec.clear();
    }
}

void BoundaryContext::EndSetBoundary() {
    for (int d = 0; d < 4; ++d) {
        uint32_t cell_count = d <= 1 ? v_cell_count_ : h_cell_count_;
        uint32_t buf_size = d <= 1 ? v_buf_size_ : h_buf_size_;
        buffer_size_[d] = buf_size;
        //默认边界条件：无滑移墙壁
        std::vector<uint8_t> bytes(buf_size, 0);
        //覆盖自定义边界条件
        for (const auto& seg_vec = segments_[d]; const auto&[bp, ep, type, u, v] : seg_vec) {
            uint32_t begin = static_cast<int>(bp * static_cast<float>(cell_count));
            uint32_t end = static_cast<int>(ep * static_cast<float>(cell_count));
            uint32_t off = begin * kBufferUnitSize;
            auto uint_type = static_cast<uint32_t>(type);
            for (uint32_t i = begin; i < end; ++i) {
                auto* p = bytes.data() + off;
                memcpy(p, &uint_type, sizeof(uint32_t));
                memcpy(p + sizeof(uint32_t), &u, sizeof(float));
                memcpy(p + sizeof(uint32_t) + sizeof(float), &v, sizeof(float));
                off += kBufferUnitSize;
            }
        }
        UploadBufferData(dm_->device(), dm_->physical_device(), dm_->command_pool(),
            dm_->graphics_queue(), bytes, *buffer_bc_info_[d]);
    }
}
