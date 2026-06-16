//
// Created by PC on 2026/6/11.
//

#include "boundary_conditions.h"

#include "case_data.h"
#include "vulkan/device_manager.h"
#include "vulkan/buffers.h"

namespace rtfs2d {

void BoundaryConditions::SetBoundary(BoundaryDirection dir, BoundaryType type,
        float begin, float end, float u, float v) {
    int idx = static_cast<int>(dir);
    segments_[idx].push_back({begin, end, type, u, v});
}

void BoundaryConditions::Reset() {
    for (auto& seg_vec : segments_) {
        seg_vec.clear();
    }
}

void BoundaryConditions::UploadData(const CaseData& cd, DeviceManager& dm) const {
    uint32_t h_cell_count_ = cd.nx() - 2;
    uint32_t v_cell_count_ = cd.ny() - 2;
    uint32_t h_buf_size_ = h_cell_count_ * kBufferUnitSize;
    uint32_t v_buf_size_ = v_cell_count_ * kBufferUnitSize;
    for (int d = 0; d < 4; ++d) {
        uint32_t cell_count = d <= 1 ? v_cell_count_ : h_cell_count_;
        uint32_t buf_size = d <= 1 ? v_buf_size_ : h_buf_size_;
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
        dm.InitBuffer(buffers::kBufBc0 + d, bytes);
    }
}

}  // namespace rtfs2d
