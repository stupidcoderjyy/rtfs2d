//
// Created by PC on 2026/6/10.
//

#ifndef RTFS2D_JACOBI_SOLVER_H
#define RTFS2D_JACOBI_SOLVER_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class DeviceManager;
class ComputeContext;

class JacobiSolver {
public:
    JacobiSolver(DeviceManager& dm, ComputeContext& cc);
    void RecordCommands(const vk::raii::CommandBuffer& cb, float alpha, float beta) const;
private:
    DeviceManager* dm_;
    ComputeContext* cc_;
    std::unique_ptr<vk::raii::PipelineLayout> pipeline_layout_;
    std::unique_ptr<vk::raii::Pipeline> pipeline_;
};

}



#endif //RTFS2D_JACOBI_SOLVER_H
