//
// Created by PC on 2026/6/11.
//

#ifndef RTFS2D_PRESSURE_BC_SOLVER_H
#define RTFS2D_PRESSURE_BC_SOLVER_H

#include <vulkan/vulkan_raii.hpp>
#include <memory>

namespace rtfs2d {

class DeviceManager;
class ComputeContext;

class PressureBCSolver {
public:
    PressureBCSolver(DeviceManager& dm, ComputeContext& cc);
    void RecordCommands(const vk::raii::CommandBuffer& cb, const vk::raii::DescriptorSet& ds) const;

private:
    DeviceManager* dm_;
    ComputeContext* cc_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_;
};

}

#endif //RTFS2D_PRESSURE_BC_SOLVER_H
