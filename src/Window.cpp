//
// Created by PC on 2026/6/5.
//

#include "Window.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace rtfs2d;

Window::Window(int width, int height, std::string title):
        width_(width), height_(height), title_(std::move(title)) {}

void Window::Show() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    auto glfw_guard = std::shared_ptr<void>(nullptr, [](...) {
        glfwTerminate();
    });
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    CreateVkInstance();
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    auto window_guard = std::shared_ptr<void>(nullptr, [this](...) {
        glfwDestroyWindow(window_);
    });
    // 主循环
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
}

void Window::CreateVkInstance() {
    // 获取 GLFW 需要的扩展
    uint32_t ext_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&ext_count);
    if (!extensions || ext_count == 0) {
        throw std::runtime_error("GLFW requires at least one Vulkan extension");
    }
    std::vector enabled_extensions(extensions, extensions + ext_count);

    // 检查验证层是否可用
    constexpr std::string_view validation_layer = "VK_LAYER_KHRONOS_validation";
    bool layer_available = false;
    try {
        for (auto layer_props = vk::enumerateInstanceLayerProperties(); const auto& prop : layer_props) {
            if (validation_layer == prop.layerName) {
                layer_available = true;
                break;
            }
        }
    } catch (const vk::SystemError& e) {
        std::cerr << "Failed to enumerate instance layers: " << e.what() << std::endl;
        // 继续，假设没有验证层
    }

    std::vector<const char*> enabled_layers;
    if (layer_available) {
        enabled_layers.push_back(validation_layer.data());
        std::cout << "Validation layer enabled" << std::endl;
    } else {
        std::cout << "Validation layer not available" << std::endl;
    }

    // 填充 VkApplicationInfo（使用指定初始化器，C++20）
    vk::ApplicationInfo app_info(
        title_.c_str(),
        VK_MAKE_VERSION(1, 0, 0),
        "rtfs2d",
        VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_3
    );

    // 填充 InstanceCreateInfo
    vk::InstanceCreateInfo create_info;
    create_info.setPApplicationInfo(&app_info)
              .setEnabledLayerCount(static_cast<uint32_t>(enabled_layers.size()))
              .setPpEnabledLayerNames(enabled_layers.empty() ? nullptr : enabled_layers.data())
              .setEnabledExtensionCount(static_cast<uint32_t>(enabled_extensions.size()))
              .setPpEnabledExtensionNames(enabled_extensions.data());

    // 创建 RAII 实例（自动管理 vkDestroyInstance）
    try {
        instance_ = std::make_unique<vk::raii::Instance>(vk::raii::Context(), create_info);
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(std::string("Failed to create Vulkan instance: ") + e.what());
    }
}
