//
// Created by PC on 2026/6/5.
//

#ifndef RTFS2D_WINDOW_H
#define RTFS2D_WINDOW_H
#include <string>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class Window {
public:
    Window(int width, int height, std::string title);
    void Show();
private:
    int width_, height_;
    std::string title_;
    GLFWwindow* window_{};
    std::unique_ptr<vk::raii::Instance> instance_;

    void CreateVkInstance();
};

}

#endif //RTFS2D_WINDOW_H
