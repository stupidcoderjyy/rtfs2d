//
// Created by PC on 2026/6/5.
//

#ifndef RTFS2D_WINDOW_H
#define RTFS2D_WINDOW_H
#include <string>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "vk_swapchain.h"
#include "vk_device.h"
#include "vk_compute.h"
#include "vk_graphics.h"

namespace rtfs2d {

class Window {
public:
    explicit Window(std::string title, bool debug_enabled = true);
    void Show();
private:
    int width_, height_;
    std::string title_;
    GLFWwindow* window_{};
    std::unique_ptr<DeviceManager> device_manager_;
    std::unique_ptr<SwapchainContext> swapchain_ctx_;
    std::unique_ptr<ComputeContext> compute_ctx_;
    std::unique_ptr<GraphicsContext> graphics_ctx_;
    bool debug_enabled_;
    size_t current_frame_ = 0;

    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
};

}

#endif //RTFS2D_WINDOW_H
