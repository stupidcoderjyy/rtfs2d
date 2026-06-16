//
// Created by PC on 2026/6/15.
//

#include "buffers.h"

#include "case_data.h"
#include "vk_device.h"
#include "grid.h"
#include "obstacle_geometry.h"
#include "vk_boundary.h"

namespace rtfs2d {

void buffers::InitBuffers(DeviceManager& dm, const CaseData& case_data) {
    auto cell_count = case_data.total_cells();
    auto field_buf_size = cell_count * sizeof(float);
    std::vector host_data(cell_count, 0.0f);
    auto usage = vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eTransferSrc
        | vk::BufferUsageFlagBits::eTransferDst;
    auto prop = vk::MemoryPropertyFlagBits::eDeviceLocal;
    for (int i = kBufV0; i <= kBufV6; ++i) {
        dm.CreateBuffer(i, field_buf_size, usage, prop);
        dm.InitBuffer(i, host_data);
    }
    // 边界条件
    usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
    prop = vk::MemoryPropertyFlagBits::eDeviceLocal;
    uint32_t h_buf_size_ = case_data.nx() * BoundaryConditions::kBufferUnitSize;
    uint32_t v_buf_size_ = case_data.ny() * BoundaryConditions::kBufferUnitSize;
    for (int i = kBufBc0; i <= kBufBc3; ++i) {
        auto buf_size = i < kBufBc2 ? v_buf_size_ : h_buf_size_;
        dm.CreateBuffer(i, buf_size, usage, prop);
        //默认边界条件：无滑移墙壁
        std::vector<uint8_t> initial(buf_size, 0); // kNoSlipWall + 0.0f + 0.0f
        dm.InitBuffer(i, initial);
    }
    // 创建多边形 SSBO
    dm.CreateBuffer(kBufIbmPolygon, ObstacleGeometry::kPolygonBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    dm.CreateBuffer(kBufIbmMarker, ObstacleGeometry::kMarkBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    dm.CreateBuffer(kBufIbmForce, 2 * cell_count * sizeof(float),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    dm.CreateBuffer(kBufIbmMask, 2 * cell_count * sizeof(float),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    // 初始化为零
    dm.InitBuffer<uint8_t>(kBufIbmPolygon, 0);
    dm.InitBuffer<uint8_t>(kBufIbmMarker, 0);
    dm.InitBuffer<uint8_t>(kBufIbmForce, 0);
    dm.InitBuffer<uint8_t>(kBufIbmMask, 0);
}

}
