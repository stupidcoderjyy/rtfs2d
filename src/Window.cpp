//
// Created by PC on 2026/6/5.
//

#include "Window.h"

#include <spdlog/spdlog.h>
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
    CreateVkInstance();
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    auto window_guard = std::shared_ptr<void>(nullptr, [this](...) {
        glfwDestroyWindow(window_);
    });
    CreateWindowSurface();
    CheckPhysicalDevice();
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
    spdlog::warn("[Vulkan] {}", data->pMessage);
    return VK_FALSE;
}

void Window::CreateVkInstance() {
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
        spdlog::error("Failed to enumerate instance extensions: {}", e.what());
    }

    if (debug_supported) {
        enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        spdlog::info("VK_EXT_debug_utils enabled.");
    } else {
        spdlog::warn("VK_EXT_debug_utils not supported; debug messenger will be disabled.");
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
        spdlog::error("Failed to enumerate instance layers: {}", e.what());
        // 继续，假设没有验证层
    }

    std::vector<const char*> enabled_layers;
    if (layer_available) {
        enabled_layers.push_back(kValidationLayer);
        spdlog::info("Validation layer enabled");
    } else {
        spdlog::warn("Validation layer not available, debug mode disabled");
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
    debug_info.setMessageSeverity(
                SeverityFlag::eWarning |
                SeverityFlag::eError)
            .setMessageType(
                MsgTypeFlag::eGeneral |
                MsgTypeFlag::eValidation |
                MsgTypeFlag::ePerformance)
            .setPfnUserCallback(DebugCallback);
    return debug_info;
}

void Window::CreateWindowSurface() {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(**instance_, window_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface_ = std::make_unique<vk::raii::SurfaceKHR>(*instance_, surface);
}

//枚举物理设备并选定满足渲染需求的 GPU
void Window::CheckPhysicalDevice() {
    for (auto device : instance_->enumeratePhysicalDevices()) {
        //GPU 将功能按"族"分组。每个族内可以分配若干队列
        auto queue_families = device.getQueueFamilyProperties();
        std::optional<uint32_t> graphics_index, present_index;
        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphics_index = i;
            }
            //查指定队列族是否支持向窗口表面呈现图像
            if (device.getSurfaceSupportKHR(i, **surface_)) {
                present_index = i;
            }
        }
        // 向窗口渲染图像需要设备支持 `VK_KHR_SWAPCHAIN_EXTENSION_NAME
        bool has_swapchain = false;
        for (const auto& ext : device.enumerateDeviceExtensionProperties()) {
            if (std::strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, ext.extensionName) == 0) {
                has_swapchain = true;
                break;
            }
        }
        if (has_swapchain && graphics_index.has_value() && present_index.has_value()) {
            physical_device_ = device;
            graphics_queue_family_index_ = graphics_index.value();
            present_queue_family_index_ = present_index.value();
            spdlog::info("Found suitable physical device: {}", std::string(device.getProperties().deviceName));
            return;
        }
    }
    throw std::runtime_error("No suitable physical device found");
}
