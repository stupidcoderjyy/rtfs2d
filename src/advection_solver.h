//
// Created by PC on 2026/6/10.
//

#ifndef RTFS2D_ADVECTION_SOLVER_H
#define RTFS2D_ADVECTION_SOLVER_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class DeviceManager;
class ComputeContext;

class AdvectionSolver {
public:
    AdvectionSolver(DeviceManager& dm, ComputeContext& cc);
    void RecordCommands(const vk::raii::CommandBuffer& cb) const;
private:
    DeviceManager* dm_;
    ComputeContext* cc_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_;
};

}



#endif //RTFS2D_ADVECTION_SOLVER_H
