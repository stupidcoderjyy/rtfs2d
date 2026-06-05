//
// Created by PC on 2026/6/5.
//

#include "Window.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace rtfs2d;

Window::Window(int width, int height, std::string title, bool debug_enabled):
        width_(width), height_(height), title_(std::move(title)), debug_enabled_(debug_enabled) {}

void Window::Show() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    auto glfw_guard = std::shared_ptr<void>(nullptr, [](...) {
        glfwTerminate();
    });
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    VkCreateInstance();
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    auto window_guard = std::shared_ptr<void>(nullptr, [this](...) {
        glfwDestroyWindow(window_);
    });
    // 主循环
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* data,
        void* user_data) {
    std::cerr << "[Vulkan] " << data->pMessage << std::endl;
    return VK_FALSE;
}

void Window::VkCreateInstance() {
    auto enabled_extensions = GetEnabledExtensions();
    auto enabled_layers = GetEnabledValidationLayers();
    auto app_info = GetApplicationInfo();
    auto debug_info = GetDebugMessengerCreateInfo();

    // 填充 InstanceCreateInfo
    vk::InstanceCreateInfo create_info;
    create_info.setPApplicationInfo(&app_info)
            .setEnabledLayerCount(static_cast<uint32_t>(enabled_layers.size()))
            .setPpEnabledLayerNames(enabled_layers.empty() ? nullptr : enabled_layers.data())
            .setEnabledExtensionCount(static_cast<uint32_t>(enabled_extensions.size()))
            .setPpEnabledExtensionNames(enabled_extensions.data())
            .setPNext(&debug_info);

    // 创建 RAII 实例
    try {
        instance_ = std::make_unique<vk::raii::Instance>(vk::raii::Context(), create_info);
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(std::string("Failed to create Vulkan instance: ") + e.what());
    }
    if (debug_enabled_) {
        debug_messenger_ = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(
                instance_->createDebugUtilsMessengerEXT(debug_info));
    }
}

std::vector<const char*> Window::GetEnabledExtensions() {
    // 获取 GLFW 需要的扩展
    uint32_t ext_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&ext_count);
    if (!extensions || ext_count == 0) {
        throw std::runtime_error("GLFW requires at least one Vulkan extension");
    }
    std::vector enabled_extensions(extensions, extensions + ext_count);
    // 调试扩展
    if (!debug_enabled_) {
        return enabled_extensions;
    }
    bool debug_supported = false;
    try {
        for (auto aes = vk::enumerateInstanceExtensionProperties(); const auto& ext : aes) {
            if (!std::strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ext.extensionName)) {
                debug_supported = true;
                break;
            }
        }
    } catch (const vk::SystemError& e) {
        std::cerr << "[ERROR] Failed to enumerate instance extensions: " << e.what() << std::endl;
    }

    if (debug_supported) {
        enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::cout << "[INFO] VK_EXT_debug_utils enabled." << std::endl;
    } else {
        std::cerr << "[WARN] VK_EXT_debug_utils not supported; debug messenger will be disabled." << std::endl;
        debug_enabled_ = false;
    }
    return enabled_extensions;
}

std::vector<const char*> Window::GetEnabledValidationLayers() {
    if (!debug_enabled_) {
        return {};
    }
    // 检查验证层是否可用
    bool layer_available = false;
    constexpr auto kValidationLayer = "VK_LAYER_KHRONOS_validation";
    try {
        for (auto layer_props = vk::enumerateInstanceLayerProperties(); const auto& prop : layer_props) {
            if (!std::strcmp(kValidationLayer, prop.layerName)) {
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
        enabled_layers.push_back(kValidationLayer);
        std::cout << "[INFO] Validation layer enabled" << std::endl;
    } else {
        std::cout << "[WARN] Validation layer not available, debug mode disabled" << std::endl;
        debug_enabled_ = false;
    }
    return enabled_layers;
}

vk::ApplicationInfo Window::GetApplicationInfo() const {
    return {
        title_.c_str(),
        VK_MAKE_VERSION(1, 0, 0),
        "rtfs2d",
        VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_3
    };
}

vk::DebugUtilsMessengerCreateInfoEXT Window::GetDebugMessengerCreateInfo() const {
    if (!debug_enabled_) {
        return {};
    }
    vk::DebugUtilsMessengerCreateInfoEXT debug_info{};
    using SeverityFlag = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MsgTypeFlag = vk::DebugUtilsMessageTypeFlagBitsEXT;
    debug_info.setMessageSeverity(SeverityFlag::eWarning | SeverityFlag::eError)
            .setMessageType(MsgTypeFlag::eGeneral | MsgTypeFlag::eValidation | MsgTypeFlag::ePerformance)
            .setPfnUserCallback(DebugCallback);
    return debug_info;
}
