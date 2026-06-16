//
// Created by PC on 2026/6/5.
//

#ifndef RTFS2D_WINDOW_H
#define RTFS2D_WINDOW_H
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "vulkan/swapchain_context.h"
#include "vulkan/device_manager.h"
#include "solver/compute_context.h"
#include "render/graphics_context.h"
#include "imgui_vulkan_context.h"

namespace rtfs2d {

class Window {
public:
    explicit Window(std::unique_ptr<CaseData> case_data, bool debug_enabled = true);
    void Show();
private:
    int width_, height_;
    GLFWwindow* window_{};
    std::unique_ptr<DeviceManager> device_manager_;
    std::unique_ptr<SwapchainContext> swapchain_ctx_;
    std::unique_ptr<ComputeContext> compute_ctx_;
    std::unique_ptr<GraphicsContext> graphics_ctx_;
    std::unique_ptr<ImGuiVulkanContext> imgui_ctx_;
    std::unique_ptr<CaseData> case_data_;
    bool debug_enabled_;
    size_t current_frame_ = 0;

    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
};

}

#endif //RTFS2D_WINDOW_H
