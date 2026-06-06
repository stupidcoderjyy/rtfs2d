//
// Created by PC on 2026/6/5.
//

#include "Window.h"

#include <spdlog/spdlog.h>
#include <memory>
#include <set>
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
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateFrameBuffers();
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
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        spdlog::warn("[Vulkan] {}", data->pMessage);
    } else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        spdlog::error("[Vulkan] {}", data->pMessage);
    }
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

//从选定的物理设备创建逻辑设备并获取队列句柄
void Window::CreateLogicalDevice() {
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
    vk::DeviceCreateInfo dci{};
    dci.setQueueCreateInfoCount(queue_create_infos.size())
        .setQueueCreateInfos(queue_create_infos)
        .setEnabledExtensionCount(enabled_extensions.size())
        .setPEnabledExtensionNames(enabled_extensions)
        .setPEnabledFeatures(nullptr);
    device_ = std::make_unique<vk::raii::Device>(physical_device_, dci);
    graphics_queue_ = std::make_unique<vk::raii::Queue>(
        device_->getQueue(graphics_queue_family_index_, 0));
    present_queue_ = std::make_unique<vk::raii::Queue>(
        device_->getQueue(present_queue_family_index_, 0));
}

void Window::CreateSwapChain() {
    vk::SwapchainCreateInfoKHR ci{};

    // 指定交换链渲染的目标表面
    ci.setSurface(*surface_);

    // 查询 surface 的能力上限（最小/最大图像数量、当前窗口尺寸等）
    auto capabilities = physical_device_.getSurfaceCapabilitiesKHR(*surface_);
    vk::Extent2D ext = capabilities.currentExtent;  // 当前窗口尺寸
    // 若窗口系统不提供固定尺寸（currentExtent 为 UINT32_MAX），则用构造参数手动 clamp
    if (ext.width == UINT32_MAX) {
        ext.width = std::clamp(
            static_cast<uint32_t>(width_),
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        ext.height = std::clamp(
            static_cast<uint32_t>(height_),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }
    swapchain_extent_ = ext;
    ci.setImageExtent(swapchain_extent_);

    // 查询 surface 支持的像素格式和色彩空间，优先选择 sRGB
    auto formats = physical_device_.getSurfaceFormatsKHR(*surface_);
    auto it = std::ranges::find_if(formats, [](const auto& fmt) {
        return fmt.format == vk::Format::eB8G8R8A8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    // 若首选格式不存在，取列表第一个作为降级方案
    vk::SurfaceFormatKHR format = it != formats.end() ? *it : formats[0];
    swapchain_image_format_ = format.format;
    swapchain_color_space_ = format.colorSpace;
    ci.setImageFormat(swapchain_image_format_).setImageColorSpace(swapchain_color_space_);

    // 选择 FIFO 呈现模式（V-Sync），所有 Vulkan 实现必须支持
    constexpr auto present_mode = vk::PresentModeKHR::eFifo;
    if (auto modes = physical_device_.getSurfacePresentModesKHR(*surface_);
            std::ranges::find(modes, present_mode) == modes.end()) {
        throw std::runtime_error("No suitable present mode found");
    }
    ci.setPresentMode(present_mode);

    // 图像层数，普通 2D 应用设为 1
    ci.setImageArrayLayers(1);

    // 最少图像数量：minImageCount + 1 以减少等待，但不超 maxImageCount
    uint32_t min_img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && min_img_count > capabilities.maxImageCount) {
        min_img_count = capabilities.maxImageCount;
    }
    ci.setMinImageCount(min_img_count);

    // 交换链图像将作为颜色附件进行渲染
    ci.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    // 使用 surface 的当前变换（旋转等），不做额外处理
    ci.setPreTransform(capabilities.currentTransform);

    // 窗口与桌面合成时不透明
    ci.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);

    // 允许 Vulkan 裁剪被遮挡的像素
    ci.setClipped(VK_TRUE);

    // 若 graphics 和 present 是同一队列族，使用独占模式；否则使用并发模式
    if (graphics_queue_family_index_ == present_queue_family_index_) {
        ci.setImageSharingMode(vk::SharingMode::eExclusive);
    } else {
        uint32_t indices[] = { graphics_queue_family_index_, present_queue_family_index_ };
        ci.setImageSharingMode(vk::SharingMode::eConcurrent).setQueueFamilyIndices(indices);
    }

    // 创建 RAII 交换链，析构时自动销毁
    swapchain_ = std::make_unique<vk::raii::SwapchainKHR>(*device_, ci);
    // 获取交换链中的图像数组
    swapchain_images_ = swapchain_->getImages();
}

void Window::CreateImageViews() {
    for (const auto& img : swapchain_images_) {
        vk::ImageViewCreateInfo ci{};
        ci.setImage(img)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(swapchain_image_format_)
            .setSubresourceRange({
                vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
            });
        swapchain_image_views_.push_back(
            std::make_unique<vk::raii::ImageView>(device_->createImageView(ci))
        );
    }
}

void Window::CreateRenderPass() {
    // 定义一个颜色附件：像素格式与交换链一致
    vk::AttachmentDescription attachments[]{{}};
    attachments[0].setFormat(swapchain_image_format_)
        .setSamples(vk::SampleCountFlagBits::e1)            // 不使用多重采样
        .setLoadOp(vk::AttachmentLoadOp::eClear)            // 渲染前清空附件
        .setStoreOp(vk::AttachmentStoreOp::eStore)          // 渲染后保留结果，供呈现用
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)  // 不使用模板缓冲
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)      // 渲染前布局无关
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);   // 渲染后转为呈现布局

    // subpass 中引用附件的描述：索引 0，布局为颜色附件最优
    vk::AttachmentReference ar{};
    ar.setAttachment(0)
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    // 定义一个图形 subpass，引用上述颜色附件
    vk::SubpassDescription subpasses[]{{}};
    subpasses[0].setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&ar);

    // 组装渲染通道创建信息
    vk::RenderPassCreateInfo ci{};
    ci.setAttachmentCount(1)
        .setPAttachments(attachments)
        .setSubpassCount(1)
        .setPSubpasses(subpasses);

    // 创建 RAII 渲染通道，析构时自动销毁
    render_pass_ = std::make_unique<vk::raii::RenderPass>(device_->createRenderPass(ci));
}

void Window::CreateFrameBuffers() {
    // 为每个 swapchain image view 创建对应的帧缓冲
    for (const auto& view : swapchain_image_views_) {
        vk::FramebufferCreateInfo ci{};
        ci.setRenderPass(*render_pass_)     // 帧缓冲兼容的渲染通道
            .setAttachmentCount(1)          // 附件数量
            .setPAttachments(&**view)       // 附件列表，解引用 RAII 句柄获取底层指针
            .setWidth(swapchain_extent_.width)    // 帧缓冲宽度与交换链一致
            .setHeight(swapchain_extent_.height)  // 帧缓冲高度与交换链一致
            .setLayers(1);                  // 单层
        // 创建 RAII 帧缓冲，析构时自动销毁
        framebuffers_.push_back(
            std::make_unique<vk::raii::Framebuffer>(device_->createFramebuffer(ci))
        );
    }
}
