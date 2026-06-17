//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_COMPUTE_H
#define RTFS2D_VK_COMPUTE_H

#include <functional>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/descriptor_sets.h"
#include "fluid_solvers.h"
#include "boundary_conditions.h"
#include "render/vis_config.h"

namespace rtfs2d {

class DeviceManager;

class ComputeContext {
public:
    explicit ComputeContext(
        DeviceManager& dm,
        DescriptorSets& ds,
        const CaseData& case_data,
        VisConfig& vis_config);
    void UploadCaseData();
    void RecordCommands(const vk::raii::CommandBuffer& cb);
    void InitField() const; // 初始化流场

    void EnsureBufferReady(
        vk::PipelineStageFlagBits src_stage,
        vk::PipelineStageFlagBits dst_stage,
        const vk::raii::CommandBuffer& cb, int buf) const;

    void EnsureBufferReadyForCompute(const vk::raii::CommandBuffer& cb, int buf) const {
        EnsureBufferReady(vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader, cb, buf);
    }

    DescriptorSetIndex GetVisDescriptorSetIndex() const { return dye_use_set1_ ? kSetVis1 : kSetVis2;}
    void SetDyeInjectPos(float x, float y) { dye_x_ = x;dye_y_ = y; }
    void SetDyeInjecting(bool injecting) {dye_injecting_ = injecting;}

    int cell_count() const { return cell_count_; }
    vk::DeviceSize buf_size() const { return compute_buf_size_; }
    std::shared_ptr<vk::raii::PipelineLayout> pipeline_layout() const { return pipeline_layout_; }
    uint32_t ibm_marker_count() const { return ibm_marker_count_; }
private:
    const CaseData* case_data_;
    DeviceManager* dm_;
    DescriptorSets* ds_;
    VisConfig* vis_config_;
    std::shared_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<FluidSolvers> fluid_solvers_;
    const int cell_count_;
    const uint32_t poisson_iter_n;
    const vk::DeviceSize compute_buf_size_;
    uint32_t ibm_marker_count_{};
    bool dye_use_set1_ = false;
    // 是否正在注入染料
    bool dye_injecting_ = false;
    // 染料位置
    float dye_x_{}, dye_y_{};

    void UploadObstacles();
    void ComputeObstacles(const vk::raii::CommandBuffer& cb) const;
    void ComputeFluid(const vk::raii::CommandBuffer& cb);
    void ComputeRenderData(const vk::raii::CommandBuffer& cb) const;

    void DebugReadBackBuffer(const vk::raii::Buffer& buf, uint32_t size,
        const std::function<void(void* buf, uint32_t len)>& handler) const;
    void DebugReadBackVelocityBuffer(const vk::raii::CommandBuffer& cb,
        int buffer, const std::string& log_prefix) const;
    void DebugReadBackVelocityBufferPoints(const vk::raii::CommandBuffer &cb,
        int buffer, const std::string &log_prefix, const std::vector<int> &indexes) const;
    void DebugReadBackBoundaryBuffer(int buffer) const;
};

}  // namespace rtfs2d

#endif  // RTFS2D_VK_COMPUTE_H