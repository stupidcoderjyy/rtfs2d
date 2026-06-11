#ifndef RTFS2D_VK_BOUNDARY_H
#define RTFS2D_VK_BOUNDARY_H

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <array>

namespace rtfs2d {

class DeviceManager;
struct GridParams;

enum class BoundaryDirection : int32_t {
    kTop = 0,
    kBottom,
    kLeft,
    kRight
};

enum class BoundaryType : int32_t {
    kNone = 0,
    kNoSlipWall,
    kVelocityInlet,
    kVelocityOutlet,
    kPressureInlet,
    kPressureOutlet,
    kMovingWall
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

    void SetBoundary(
        BoundaryDirection dir,
        BoundaryType type,
        float begin = 0.0f,
        float end = 1.0f,
        float u = 0.0f,
        float v = 0.0f);

    void Upload() const;

    const vk::raii::Buffer& bc_type_buffer() const { return *bc_type_; }
    const vk::raii::Buffer& bc_vel_u_buffer() const { return *bc_vel_u_; }
    const vk::raii::Buffer& bc_vel_v_buffer() const { return *bc_vel_v_; }

    uint32_t bc_offset(BoundaryDirection dir) const;
    uint32_t bc_count(BoundaryDirection dir) const;

private:
    DeviceManager* dm_;
    const GridParams& grid_params_;

    uint32_t top_count_;
    uint32_t bottom_count_;
    uint32_t left_count_;
    uint32_t right_count_;
    uint32_t bc_total_count_;

    std::array<std::vector<BoundarySegment>, 4> segments_;

    std::unique_ptr<vk::raii::Buffer> bc_type_;
    std::unique_ptr<vk::raii::DeviceMemory> bc_type_memory_;
    std::unique_ptr<vk::raii::Buffer> bc_vel_u_;
    std::unique_ptr<vk::raii::DeviceMemory> bc_vel_u_memory_;
    std::unique_ptr<vk::raii::Buffer> bc_vel_v_;
    std::unique_ptr<vk::raii::DeviceMemory> bc_vel_v_memory_;
};

} // namespace rtfs2d

#endif // RTFS2D_VK_BOUNDARY_H