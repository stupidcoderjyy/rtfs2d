//
// Created by PC on 2026/6/9.
//

#include "vk_device.h"

#include <fstream>
#include <set>
#include <spdlog/spdlog.h>

using namespace rtfs2d;

DeviceManager::DeviceManager(GLFWwindow *window, bool debug_enabled): debug_enabled_(debug_enabled) {
    CreateVkInstance();
    CreateWindowSurface(window);
    CheckPhysicalDevice();
    CreateLogicalDevice();
    CreateCommandPool();
    CreateComputeCommandPool();
}

std::unique_ptr<vk::raii::ShaderModule> DeviceManager::LoadShader(const std::string& path) const {
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) {
        throw std::runtime_error("failed to open");
    }
    // 获取文件大小并检查是否为 4 的倍数
    is.seekg(0, std::ios::end);
    std::streamsize fs = is.tellg();
    if (fs % sizeof(uint32_t) != 0) {
        throw std::runtime_error("SPIR-V file size is not a multiple of 4 bytes");
    }

    std::vector<uint32_t> buffer(fs / sizeof(uint32_t));
    is.seekg(0, std::ios::beg);
    is.read(reinterpret_cast<char*>(buffer.data()), fs);

    if (is.gcount() != fs) {
        throw std::runtime_error("failed to read");
    }

    vk::ShaderModuleCreateInfo ci{};
    ci.setCodeSize(buffer.size() * sizeof(uint32_t))
        .setPCode(buffer.data());
    return std::make_unique<vk::raii::ShaderModule>(device_->createShaderModule(ci));
}

std::unique_ptr<vk::raii::Pipeline> DeviceManager::CreateComputePipelineFromFile(
        const vk::raii::PipelineLayout &layout,
        const std::string &spv_path) const {
    auto shader = LoadShader(spv_path);

    vk::PipelineShaderStageCreateInfo pss_ci{};
    pss_ci.setStage(vk::ShaderStageFlagBits::eCompute)
        .setModule(**shader)
        .setPName("main");
    vk::ComputePipelineCreateInfo cp_ci{};
    cp_ci.setStage(pss_ci)
        .setLayout(layout);
    return std::make_unique<vk::raii::Pipeline>(
        device_->createComputePipeline(nullptr, cp_ci));
}

void DeviceManager::CreateVkInstance() {
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

std::vector<const char*> DeviceManager::GetEnabledExtensions() {
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
        spdlog::info("Debug utils extension enabled");
    } else {
        spdlog::warn("VK_EXT_debug_utils not supported; debug messenger will be disabled.");
        debug_enabled_ = false;
    }
    return enabled_extensions;
}

std::vector<const char*> DeviceManager::GetEnabledValidationLayers() {
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

vk::ApplicationInfo DeviceManager::GetApplicationInfo() {
    return {
        "rtfs2d",
        VK_MAKE_VERSION(1, 0, 0),
        "rtfs2d",
        VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_3
    };
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* data,
        void* user_data) {
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        spdlog::warn("[Vulkan] {}", data->pMessage);
    } else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        spdlog::error("[Vulkan] {}", data->pMessage);
    }
    return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT DeviceManager::GetDebugMessengerCreateInfo() const {
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

void DeviceManager::CreateWindowSurface(GLFWwindow *window) {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(**instance_, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface_ = std::make_unique<vk::raii::SurfaceKHR>(*instance_, surface);
}

//枚举物理设备并选定满足渲染需求的 GPU
void DeviceManager::CheckPhysicalDevice() {
    for (const auto& device : instance_->enumeratePhysicalDevices()) {
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
            spdlog::info("Physical device: {}", std::string(device.getProperties().deviceName));
            return;
        }
    }
    throw std::runtime_error("No suitable physical device found");
}

//从选定的物理设备创建逻辑设备并获取队列句柄
void DeviceManager::CreateLogicalDevice() {
    std::set queue_families{
        graphics_queue_family_index_, present_queue_family_index_
    };
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    float priority = 1.0f;
    for (const auto& qf : queue_families) {
        vk::DeviceQueueCreateInfo ci{};
        ci.setQueueFamilyIndex(qf)
            .setQueueCount(1)   //每个族只申请一个队
            .setPQueuePriorities(&priority);
        queue_create_infos.push_back(ci);
    }
    std::vector enabled_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceFeatures df{};
    df.setFragmentStoresAndAtomics(VK_TRUE);

    vk::DeviceCreateInfo dci{};
    dci.setQueueCreateInfoCount(queue_create_infos.size())
        .setQueueCreateInfos(queue_create_infos)
        .setEnabledExtensionCount(enabled_extensions.size())
        .setPEnabledExtensionNames(enabled_extensions)
        .setPEnabledFeatures(&df);
    device_ = std::make_unique<vk::raii::Device>(physical_device_, dci);
    graphics_queue_ = std::make_unique<vk::raii::Queue>(
        device_->getQueue(graphics_queue_family_index_, 0));
    present_queue_ = std::make_unique<vk::raii::Queue>(
        device_->getQueue(present_queue_family_index_, 0));
}

void DeviceManager::CreateCommandPool() {
    // 创建命令池，指定提交到 graphics 队列族
    vk::CommandPoolCreateInfo ci{};
    ci.setQueueFamilyIndex(graphics_queue_family_index_)
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    command_pool_ = std::make_unique<vk::raii::CommandPool>(device_->createCommandPool(ci));
}

void DeviceManager::CreateComputeCommandPool() {
    vk::CommandPoolCreateInfo cp_ci{};
    cp_ci.setQueueFamilyIndex(graphics_queue_family_index_)
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    compute_command_pool_ = std::make_unique<vk::raii::CommandPool>(
        device_->createCommandPool(cp_ci));
}
