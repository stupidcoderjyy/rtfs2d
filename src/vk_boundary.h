#ifndef RTFS2D_VK_BOUNDARY_H
#define RTFS2D_VK_BOUNDARY_H

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <array>

#include "buffers.h"

namespace rtfs2d {

class DeviceManager;
struct GridParams;

enum class BoundaryDirection : uint32_t {
    kLeft = 0,
    kRight = 1,
    kBottom = 2,
    kTop = 3
};

enum class BoundaryType : int8_t {
    kNoSlipWall = 0,
    kSlipWall = 1,
    kVelocity = 2,
    kPressure = 3
};

struct BoundarySegment {
    float begin;
    float end;
    BoundaryType type;
    float u;
    float v;
};

class BoundaryContext {
public:
    BoundaryContext(DeviceManager& dm, const GridParams& gp);
    void SetBoundary(BoundaryDirection dir, BoundaryType type,
        float begin = 0.0f, float end = 1.0f, float u = 0.0f, float v = 0.0f);
    void BeginSetBoundary();
    void EndSetBoundary() const;

    // 存储边界条件的信息，每个数据单元包括 int8 + float + float
    static constexpr uint32_t kBufferUnitSize = sizeof(uint32_t) + sizeof(float) * 2;
private:
    DeviceManager* dm_;
    const GridParams& grid_params_;
    uint32_t h_cell_count_, v_cell_count_;
    uint32_t h_buf_size_, v_buf_size_;

    std::array<std::vector<BoundarySegment>, 4> segments_{};
};

} // namespace rtfs2d

#endif // RTFS2D_VK_BOUNDARY_H