//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_GRAPHICS_H
#define RTFS2D_VK_GRAPHICS_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class DeviceManager;
class SwapchainContext;
class ComputeContext;
class DescriptorSets;
class CaseData;
class VisConfig;

class GraphicsContext {
public:
    GraphicsContext(DeviceManager& dm, SwapchainContext& sc, ComputeContext& cc, DescriptorSets& ds, const CaseData& cd);
    void BeginRenderPass(const vk::raii::CommandBuffer& cb, uint32_t img_idx) const;
    void EndRenderPass(const vk::raii::CommandBuffer& cb, uint32_t img_idx) const;
    void RecordCommands(const vk::raii::CommandBuffer &cb, VisConfig& vis_config) const;
private:
    DeviceManager* dm_;
    SwapchainContext* sc_;
    ComputeContext* cc_;
    DescriptorSets* ds_;
    const CaseData* case_data_;
    std::unique_ptr<vk::raii::PipelineLayout> graphics_pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> graphics_pipeline_;

    void CreateGraphicsPipeline();
};

}

#endif //RTFS2D_VK_GRAPHICS_H