//
// Created by PC on 2026/6/11.
//

#ifndef RTFS2D_VORTICITY_SOLVER_H
#define RTFS2D_VORTICITY_SOLVER_H

#include <vulkan/vulkan_raii.hpp>
#include <memory>

namespace rtfs2d {

class DeviceManager;
class ComputeContext;

class VorticitySolver {
public:
    VorticitySolver(DeviceManager& dm, ComputeContext& cc);
    ~VorticitySolver() = default;

    void RecordCommands(const vk::raii::CommandBuffer& cb,
        const vk::raii::DescriptorSet& ds,
        float epsilon) const;

private:
    DeviceManager* dm_;
    ComputeContext* cc_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_;
};

} // namespace rtfs2d



#endif //RTFS2D_VORTICITY_SOLVER_H
