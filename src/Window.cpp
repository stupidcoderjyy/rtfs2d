//
// Created by PC on 2026/6/5.
//

#include "Window.h"
#include <GLFW/glfw3.h>

using namespace rtfs2d;

Window::Window(int width, int height, std::string title):
        width_(width), height_(height), title_(std::move(title)) {}

void Window::Show() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
    glfwDestroyWindow(window_);
}
