//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_COMPUTE_H
#define RTFS2D_VK_COMPUTE_H

#include <functional>
#include <vulkan/vulkan_raii.hpp>

#include "grid.h"
#include "fluid_solvers.h"
#include "vk_boundary.h"
#include "obstacle_geometry.h"

namespace rtfs2d {

class DeviceManager;

//TODO 这个应当放入配置模块
enum class VisField { kSpeed = 0, kPressure = 1, kVorticity = 2 };
enum class VisGradient { kGray = 0, kJet = 1, kCoolWarm = 2 };

class ComputeContext {
public:
    explicit ComputeContext(DeviceManager& dm, const GridParams& params);
    void RecordAndSubmit(const vk::raii::CommandBuffer& cb) const;

    //TODO Buffer、DescriptorSet是整个程序共用的，应当分离成独立的模块
    enum DescriptorSetIndex {
        kSetAdvection,              // 平流
        kSetVorticity,              // 涡量约束
        kSetDiffusionEven,          // 扩散 u 分量偶次迭代
        kSetDiffusionOdd,           // 扩散 u 分量奇次迭代
        kSetDivergence,             // 散度计算
        kSetPressureEven,           // 压力求解偶次迭代
        kSetPressureOdd,            // 压力求解奇次迭代
        kSetProjection,             // 压力投影修正速度
        kSetIbmApplyForce,
        kSetIbmInterpolate,
        kSetIbmMask,
        kSetIbmSpreadMarkers,
        kSetSmoothVelocity,         // 平滑速度场（仅用于漩涡强度）
        kSetComputeScalarVI,        // 漩涡强度计算
        kSetComputeScalarOthers,    // 其他可视化标量计算
        kSetVisualization           // 可视化
    };

    void EnsureBufferReady(
        vk::PipelineStageFlagBits src_stage,
        vk::PipelineStageFlagBits dst_stage,
        const vk::raii::CommandBuffer& cb, int buf) const;

    void EnsureBufferReadyForCompute(const vk::raii::CommandBuffer& cb, int buf) const {
        EnsureBufferReady(vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader, cb, buf);
    }

    // getter
    const vk::raii::DescriptorSet& DescriptorSetAt(DescriptorSetIndex idx) const {
        return *descriptor_sets_[idx];
    }
    const vk::raii::DescriptorSetLayout& descriptor_set_layout() const { return *descriptor_set_layout_; }
    const GridParams& grid_params() const { return grid_params_; }
    int cell_count() const { return compute_cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }
    const vk::raii::PipelineLayout& pipeline_layout() const { return *pipeline_layout_; }
    BoundaryContext& boundary_ctx() const { return *boundary_ctx_; }
    uint32_t ibm_marker_count() const { return ibm_marker_count_; }
    VisField vis_field() const { return vis_field_; }
    VisGradient vis_gradient() const { return vis_gradient_; }

    //setter
    void set_vis_field(VisField field) { vis_field_ = field; }
    void set_vis_gradient(VisGradient gradient) { vis_gradient_ = gradient; }
private:
    DeviceManager* dm_;
    // v0, v1, v2, v3, v4, v5, bc1, bc2, bc3, bc4, poly, marker, force, mask
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::DescriptorPool> descriptor_pool_;
    std::vector<std::unique_ptr<vk::raii::DescriptorSet>> descriptor_sets_;
    std::unique_ptr<FluidSolvers> fluid_solvers_;
    GridParams grid_params_;
    const int compute_cell_count_;
    const vk::DeviceSize compute_buf_size_;
    std::unique_ptr<BoundaryContext> boundary_ctx_;
    uint32_t ibm_marker_count_{};
    VisField vis_field_;
    VisGradient vis_gradient_;

    void CreateBuffers() const;
    void CreateDescriptorSets();
    void CreatePipelineLayout();
    void RecordFluidStepCommands(const vk::raii::CommandBuffer& cb) const;
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