//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_COMPUTE_H
#define RTFS2D_VK_COMPUTE_H

#include <functional>
#include <vulkan/vulkan_raii.hpp>

#include "descriptor_sets.h"
#include "grid.h"
#include "fluid_solvers.h"
#include "vk_boundary.h"
#include "obstacle_geometry.h"

namespace rtfs2d {

class DeviceManager;

//TODO 这个应当放入配置模块
enum class VisField { kSpeed = 0, kPressure = 1, kVorticity = 2 };
enum class VisGradient { kGray = 0, kJet = 1, kCoolWarm = 2 };
enum class VisMode { kField = 0, kDye = 1 };

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm, DescriptorSets& ds, const GridParams& params);
    void RecordCommands(const vk::raii::CommandBuffer& cb);

    void EnsureBufferReady(
        vk::PipelineStageFlagBits src_stage,
        vk::PipelineStageFlagBits dst_stage,
        const vk::raii::CommandBuffer& cb, int buf) const;

    void EnsureBufferReadyForCompute(const vk::raii::CommandBuffer& cb, int buf) const {
        EnsureBufferReady(vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader, cb, buf);
    }

    DescriptorSetIndex GetVisDescriptorSetIndex() const {
        return dye_use_set1_ ? kSetVis1 : kSetVis2;
    }

    void SetDyeInjectPos(float x, float y) {
        dye_x_ = x;
        dye_y_ = y;
    }

    void SetDyeInjecting(bool injecting) {
        dye_injecting_ = injecting;
    }

    const GridParams& grid_params() const { return grid_params_; }
    int cell_count() const { return cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }
    std::shared_ptr<vk::raii::PipelineLayout> pipeline_layout() const { return pipeline_layout_; }
    BoundaryContext& boundary_ctx() const { return *boundary_ctx_; }
    uint32_t ibm_marker_count() const { return ibm_marker_count_; }
    VisField vis_field() const { return vis_field_; }
    VisGradient vis_gradient() const { return vis_gradient_; }
    VisMode vis_mode() const { return vis_mode_; }

    //setter
    void set_vis_field(VisField field) { vis_field_ = field; }
    void set_vis_gradient(VisGradient gradient) { vis_gradient_ = gradient; }
    void set_vis_mode(VisMode mode) { vis_mode_ = mode; }
private:
    DeviceManager* dm_;
    DescriptorSets* ds_;
    // v0, v1, v2, v3, v4, v5, bc1, bc2, bc3, bc4, poly, marker, force, mask
    std::shared_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<FluidSolvers> fluid_solvers_;
    GridParams grid_params_;
    const int cell_count_;
    const uint32_t poisson_iter_n;
    const vk::DeviceSize compute_buf_size_;
    std::unique_ptr<BoundaryContext> boundary_ctx_;
    uint32_t ibm_marker_count_{};
    VisField vis_field_ = VisField::kVorticity;
    VisGradient vis_gradient_ = VisGradient::kJet;
    VisMode vis_mode_ = VisMode::kField;
    bool dye_use_set1_ = false;
    // 是否正在注入染料
    bool dye_injecting_ = false;
    // 染料位置
    float dye_x_{}, dye_y_{};

    void InitializeVortexField() const; // 初始化流场为涡旋场
    void UploadObstacles(const ObstacleGeometry& geom);

    void DebugReadBackBuffer(const vk::raii::Buffer& buf, uint32_t size,
        const std::function<void(void* buf, uint32_t len)>& handler) const;
    void DebugReadBackVelocityBuffer(const vk::raii::CommandBuffer& cb,
        int buffer, const std::string& log_prefix) const;
    void DebugReadBackVelocityBufferPoints(const vk::raii::CommandBuffer &cb,
        int buffer, const std::string &log_prefix, const std::vector<int> &indexes) const;
    void DebugReadBackBoundaryBuffer(int buffer) const;
    void AddDebugGeometry();
};

}  // namespace rtfs2d

#endif  // RTFS2D_VK_COMPUTE_H