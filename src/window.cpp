//
// Created by PC on 2026/6/5.
//

#include "window.h"

#include <fstream>
#include <spdlog/spdlog.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

using namespace rtfs2d;

Window::Window(int width, int height, std::string title, bool debug_enabled):
        width_(width), height_(height), title_(std::move(title)), debug_enabled_(debug_enabled) {}

void Window::Show() {
    try {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        auto glfw_guard = std::shared_ptr<void>(nullptr, [](...) {
            glfwTerminate();
        });
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        CreateVkInstance();
        window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* win, int w, int h) {
            auto* rtfs_win = static_cast<Window*>(glfwGetWindowUserPointer(win));
            rtfs_win->frame_buffer_resized_ = true;
            rtfs_win->width_ = w;
            rtfs_win->height_ = h;
        });
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
        CreateCommandPoolAndBuffers();
        CreateStorageBuffer();
        CreateDescriptorSetLayout();
        CreateFrameBasedSyncObjects();
        CreateImageBasedSyncObjects();

        // 加载计算2倍浮点数的着色器程序
        compute_shader_module_ = LoadShader("shaders/compute.comp.spv");
        CreatePipelineLayout();
        CreateComputePipeline();
        CreateDescriptorPool();
        CreateDescriptorSet();
        CreateComputeCommandPool();
        RecordComputeCommands();

        // 创建波浪界面的图形管线
        CreateGraphicsPipeline();

        // 主循环
        while (!glfwWindowShouldClose(window_)) {
            if (frame_buffer_resized_) {
                if (width_ == 0 || height_ == 0) {
                    glfwWaitEvents();   //最小化时阻塞当前线程，恢复窗口后再重建交换链
                } else {
                    RecreateSwapChain();
                }
                continue;
            }
            auto& cb = command_buffers_[current_frame_];
            auto& fence = in_flight_fences_[current_frame_];
            auto& acquire_sem = image_available_semaphores_[current_frame_];

            // 当CPU运行速度过快，达到kMaxFramesInFlight的限制时，必须等待GPU完成该帧的渲染，才能进行下一次提交
            // 需要为每个飞行帧准备独立的fence
            // 当渲染速度较慢时，会在这里阻塞
            if (auto result = device_->waitForFences(**in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
                    result != vk::Result::eSuccess) {
                throw std::runtime_error("waitForFences failed: " + std::to_string(static_cast<int>(result)));
            }
            device_->resetFences(**fence);

            // 完成并验证着色器计算
            vk::SubmitInfo c_si{};
            c_si.setCommandBuffers(**compute_command_buffer_);
            graphics_queue_->submit(c_si);
            graphics_queue_->waitIdle();
            if (!compute_verified_) {
                VerifyFieldData();
                compute_verified_ = true;
            }

            // 尝试从交换链获取一张处于空闲状态图像的所有权
            // 一张图片可能有六个状态：空闲状态 → 被CPU占用 → 处于渲染队列 → 正在渲染 → 处于呈现队列 → 正在呈现
            // acquire_sem用于GPU端图形队列和呈现队列之间进行同步，避免对未完成呈现（只读）的图片进行渲染（写）
            // GPU端最多只有kMaxFramesInFlight张图片处于未完成渲染的状态，acquire_sem和kMaxFramesInFlight绑定
            // 当呈现速度较慢时，会在这里阻塞
            auto [res, img_idx] = swapchain_->acquireNextImage(UINT64_MAX, **acquire_sem);
            if (res == vk::Result::eErrorOutOfDateKHR) {
                spdlog::warn("failed to acquire image from swapchain, try recreate swapchain");
                RecreateSwapChain();
                continue;
            }

            // 重新录制命令
            RecordCommands(cb, img_idx);

            // 提交命令缓冲区。GPU会等待，直到图像就绪（acquire_sem被激活），CPU端会立刻返回，并准备下一帧的内容
            // 需要为每个飞行帧准备独立的acquire_sem
            // render_sem的作用是在GPU端图形队列和呈现队列之间进行同步，避免把未完成渲染的图片输出到屏幕
            // 需要为每张交换链图像准备一个独立的render_sem
            vk::PipelineStageFlags stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            auto& render_sem = render_finished_semaphores_[img_idx];
            vk::SubmitInfo si{};
            si.setWaitSemaphores(**acquire_sem)
                .setWaitDstStageMask(stage_flags)
                .setSignalSemaphores(**render_sem)
                .setCommandBuffers(*cb);
            graphics_queue_->submit(si, **fence);

            // 将图像加入呈现队列。呈现和渲染是GPU中两个独立的模块，每张图片基于render_sem进行同步
            vk::PresentInfoKHR pi{};
            pi.setWaitSemaphores(**render_sem)
                .setSwapchains(**swapchain_)
                .setImageIndices(img_idx);
            if (auto result = present_queue_->presentKHR(pi);
                    result == vk::Result::eErrorOutOfDateKHR ||
                    result == vk::Result::eSuboptimalKHR) {
                spdlog::warn("failed to present image: {}, try recreate swapchain", static_cast<int>(result));
                frame_buffer_resized_ = true;
            }

            // 更新帧索引
            current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
            glfwPollEvents();
        }
        device_->waitIdle();
    } catch (const std::exception& e) {
        spdlog::error("Show() failed: {}", e.what());
        if (device_) {
            device_->waitIdle();
        }
        throw;
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
        spdlog::info("Debug utils extension enabled");
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

void Window::CreateSwapChain(bool replace) {
    vk::SwapchainCreateInfoKHR ci{};

    if (replace) {
        ci.setOldSwapchain(**swapchain_);
    }

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
    image_layouts_ = std::vector(swapchain_images_.size(), vk::ImageLayout::eUndefined);
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
        .setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
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

void Window::CreateCommandPoolAndBuffers() {
    // 创建命令池，指定提交到 graphics 队列族
    vk::CommandPoolCreateInfo ci{};
    ci.setQueueFamilyIndex(graphics_queue_family_index_)
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    command_pool_ = std::make_unique<vk::raii::CommandPool>(device_->createCommandPool(ci));

    // 从命令池分配命令缓冲，数量与帧缓冲一致
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(*command_pool_)
        .setCommandBufferCount(kMaxFramesInFlight)             // 和“飞行帧”数量一致
        .setLevel(vk::CommandBufferLevel::ePrimary);           // 主命令缓冲
    command_buffers_ = device_->allocateCommandBuffers(ai);
}

void Window::CreateFrameBasedSyncObjects() {
    vk::FenceCreateInfo ci{};
    ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        image_available_semaphores_[i] =
            std::make_unique<vk::raii::Semaphore>(device_->createSemaphore({}));
        in_flight_fences_[i] =
            std::make_unique<vk::raii::Fence>(device_->createFence(ci));
    }
}

void Window::CreateImageBasedSyncObjects() {
    for (int i = 0; i < swapchain_images_.size(); ++i) {
        render_finished_semaphores_.push_back(
            std::make_unique<vk::raii::Semaphore>(device_->createSemaphore({}))
        );
    }
}

void Window::RecordCommands(const vk::raii::CommandBuffer &cb, uint32_t img_idx) {
    cb.begin({});

    // 图像布局转换：从旧布局到颜色附件最优布局
    vk::ImageMemoryBarrier barrier{};
    vk::ImageSubresourceRange sr{};
    sr.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    barrier.setImage(swapchain_images_[img_idx])
        .setOldLayout(image_layouts_[img_idx])
        .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setSrcAccessMask(vk::AccessFlagBits::eNone)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
        .setSubresourceRange(sr)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, nullptr, nullptr, barrier);

    // 开始渲染通道，清除颜色为黑色
    vk::ClearValue clear_value{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
    vk::RenderPassBeginInfo bi{};
    bi.setRenderPass(*render_pass_)
        .setFramebuffer(*framebuffers_[img_idx])
        .setRenderArea({{0, 0, swapchain_extent_.width, swapchain_extent_.height}})
        .setClearValueCount(1)
        .setPClearValues(&clear_value);
    cb.beginRenderPass(bi, vk::SubpassContents::eInline);

    // 设置动态视口和剪刀（必须与交换链尺寸一致）
    vk::Viewport viewport(0.0f, 0.0f,
        static_cast<float>(swapchain_extent_.width),
        static_cast<float>(swapchain_extent_.height),
        0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, swapchain_extent_);
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    // 绑定图形管线
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, **graphics_pipeline_);

    // 将存储缓冲绑定到图形管线
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        **graphics_pipeline_layout_, 0, **descriptor_set_, nullptr);

    // 绘制全屏三角形（3个顶点）
    cb.draw(3, 1, 0, 0);

    cb.endRenderPass();

    // 将图像布局转换为呈现源布局
    cb.end();
    image_layouts_[img_idx] = vk::ImageLayout::ePresentSrcKHR;
}

void Window::RecreateSwapChain() {
    spdlog::warn("Recreate swapchain");
    device_->waitIdle();
    framebuffers_.clear();
    swapchain_image_views_.clear();
    render_finished_semaphores_.clear();
    CreateSwapChain(true);
    CreateImageViews();
    CreateFrameBuffers();
    CreateImageBasedSyncObjects();
    frame_buffer_resized_ = false;
}

std::unique_ptr<vk::raii::ShaderModule> Window::LoadShader(const std::string &path) const {
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

uint32_t Window::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    auto mp = physical_device_.getMemoryProperties();
    for (int i = 0; i < mp.memoryTypes.size(); ++i) {
        if (typeFilter & 1 << i && properties == mp.memoryTypes[i].propertyFlags) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

void Window::CreateStorageBuffer() {
    vk::BufferCreateInfo ci{};
    ci.setSize(compute_buf_size_)
        .setUsage(vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferSrc |
            vk::BufferUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive);
    scalar_field_buffer_ = std::make_unique<vk::raii::Buffer>(device_->createBuffer(ci));

    vk::DeviceBufferMemoryRequirements bmr{};
    bmr.setPCreateInfo(&ci);
    auto mr = device_->getBufferMemoryRequirements(bmr).memoryRequirements;
    auto mti = FindMemoryType(mr.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::MemoryAllocateInfo ai;
    ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    scalar_field_memory_ = std::make_unique<vk::raii::DeviceMemory>(device_->allocateMemory(ai));
    scalar_field_buffer_->bindMemory(**scalar_field_memory_, 0);

    std::unique_ptr<vk::raii::Buffer> staging_buffer;
    std::unique_ptr<vk::raii::DeviceMemory> staging_memory;
    std::vector host_data(compute_cell_count_, 0.0f);
    CreateStagingBuffer(host_data, staging_buffer, staging_memory);
    RecordStorageCommand(staging_buffer);
}

void Window::CreateStagingBuffer(
        const std::vector<float>& data,
        std::unique_ptr<vk::raii::Buffer>& staging_buffer,
        std::unique_ptr<vk::raii::DeviceMemory>& staging_memory) const {
    vk::BufferCreateInfo ci{};
    ci.setSize(compute_buf_size_)
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);
    staging_buffer = std::make_unique<vk::raii::Buffer>(device_->createBuffer(ci));

    vk::DeviceBufferMemoryRequirements bmr{};
    bmr.setPCreateInfo(&ci);
    auto mr = device_->getBufferMemoryRequirements(bmr).memoryRequirements;
    auto mti = FindMemoryType(mr.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo ai;
    ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    staging_memory = std::make_unique<vk::raii::DeviceMemory>(device_->allocateMemory(ai));

    staging_buffer->bindMemory(**staging_memory, 0);
    void* mp = staging_memory->mapMemory(0, compute_buf_size_);

    std::memcpy(mp, data.data(), compute_buf_size_);
    staging_memory->unmapMemory();
}

void Window::RecordStorageCommand(std::unique_ptr<vk::raii::Buffer> &staging_buffer) const {
    // 分配临时命令缓冲区（已在题目中给出）
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(**command_pool_)
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    std::vector<vk::raii::CommandBuffer> cbs = device_->allocateCommandBuffers(ai);
    auto& cb = cbs.front();

    // 开始命令缓冲区录制（OneTimeSubmit 标记优化）
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // 构造拷贝区域：全缓冲区拷贝，偏移为 0
    vk::BufferCopy copy_region;
    copy_region.setSrcOffset(0)
        .setDstOffset(0)
        .setSize(compute_buf_size_);

    cb.copyBuffer(**staging_buffer, **scalar_field_buffer_, copy_region);
    cb.end();

    vk::SubmitInfo si;
    si.setCommandBuffers(*cb);

    graphics_queue_->submit(si);
    graphics_queue_->waitIdle();
}

void Window::CreateDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding lb{};
    lb.setBinding(0)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eCompute |
            vk::ShaderStageFlagBits::eFragment);
    vk::DescriptorSetLayoutCreateInfo ci{};
    ci.setBindingCount(1)
        .setPBindings(&lb);
    descriptor_set_layout_ = std::make_unique<vk::raii::DescriptorSetLayout>(
        device_->createDescriptorSetLayout(ci));
}

void Window::CreatePipelineLayout() {
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&**descriptor_set_layout_);
    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        device_->createPipelineLayout(ci));
}

void Window::CreateComputePipeline() {
    vk::PipelineShaderStageCreateInfo pss_ci{};
    pss_ci.setStage(vk::ShaderStageFlagBits::eCompute)
        .setModule(**compute_shader_module_)
        .setPName("main");
    vk::ComputePipelineCreateInfo cp_ci{};
    cp_ci.setStage(pss_ci)
        .setLayout(**pipeline_layout_);
    compute_pipeline_ = std::make_unique<vk::raii::Pipeline>(
        device_->createComputePipeline(nullptr, cp_ci));
}

void Window::CreateDescriptorPool() {
    vk::DescriptorPoolSize ps{};
    ps.setType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1);
    vk::DescriptorPoolCreateInfo ci{};
    ci.setPoolSizeCount(1)
        .setPPoolSizes(&ps)
        .setMaxSets(1)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    descriptor_pool_ = std::make_unique<vk::raii::DescriptorPool>(
        device_->createDescriptorPool(ci));
}

void Window::CreateDescriptorSet() {
    vk::DescriptorSetAllocateInfo ai{};
    ai.setDescriptorSetCount(1)
        .setDescriptorPool(**descriptor_pool_)
        .setPSetLayouts(&**descriptor_set_layout_);
    auto ds = device_->allocateDescriptorSets(ai);
    descriptor_set_ = std::make_unique<vk::raii::DescriptorSet>(std::move(ds[0]));

    vk::DescriptorBufferInfo bi{};
    bi.setBuffer(**scalar_field_buffer_)
        .setOffset(0)
        .setRange(VK_WHOLE_SIZE);
    vk::WriteDescriptorSet wds{};
    wds.setDstSet(**descriptor_set_)
        .setDstBinding(0)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setPBufferInfo(&bi);
    device_->updateDescriptorSets(wds, nullptr);
}

void Window::CreateComputeCommandPool() {
    vk::CommandPoolCreateInfo cp_ci{};
    cp_ci.setQueueFamilyIndex(graphics_queue_family_index_)
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    compute_command_pool_ = std::make_unique<vk::raii::CommandPool>(
        device_->createCommandPool(cp_ci));
}

void Window::RecordComputeCommands() {
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(*compute_command_pool_)
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = device_->allocateCommandBuffers(ai);
    compute_command_buffer_ = std::make_unique<vk::raii::CommandBuffer>(std::move(cbs[0]));
    auto& cb = *compute_command_buffer_;
    cb.begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, **compute_pipeline_);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, **pipeline_layout_, 0, **descriptor_set_, nullptr);
    int group_count = (compute_cell_count_ + 127) / 128;
    cb.dispatch(group_count, 1, 1);
    vk::BufferMemoryBarrier barrier{};
    barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
        .setDstAccessMask(vk::AccessFlagBits::eHostRead)
        .setBuffer(**scalar_field_buffer_)
        .setSize(compute_buf_size_)
        .setOffset(0)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eHost,
        {}, nullptr, barrier, nullptr);
    cb.end();
}

void Window::VerifyFieldData() const {
    vk::BufferCreateInfo ci{};
    ci.setSize(compute_buf_size_)
        .setUsage(vk::BufferUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive);
    auto staging_buf = device_->createBuffer(ci);

    auto mr = staging_buf.getMemoryRequirements();
    auto mti = FindMemoryType(mr.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo m_ai{};
    m_ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    auto staging_mem = device_->allocateMemory(m_ai);
    staging_buf.bindMemory(*staging_mem, 0);

    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(*compute_command_pool_)
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = device_->allocateCommandBuffers(ai);
    auto& cb = cbs.front();
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::BufferCopy copy{};
    copy.setSrcOffset(0)
        .setSize(compute_buf_size_);
    cb.copyBuffer(**scalar_field_buffer_, *staging_buf, copy);
    cb.end();

    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    graphics_queue_->submit(si);
    graphics_queue_->waitIdle();

    auto* p_res = static_cast<float*>(staging_mem.mapMemory(0, compute_buf_size_));
    bool pass = true;
    for (int k = 0; k < compute_cell_count_; ++k) {
        int i = k % grid_params_.nx, j = k / grid_params_.nx;
        float fx = static_cast<float>(i) / static_cast<float>(grid_params_.nx);
        float fy = static_cast<float>(j) / static_cast<float>(grid_params_.ny);
        float r = std::sqrt((fx - 0.5f) * (fx - 0.5f) + (fy - 0.5f) * (fy - 0.5f)) * 2.0f;
        float expected = std::sin(r * 20.0f) * 0.5f + 0.5f;
        if (float delta = std::abs(p_res[k] - expected); delta > 1e-5f) {
            spdlog::warn("Value mismatch at ({}, {}), expected: {}, actual: {}",
                i, j, expected, p_res[k]);
            pass = false;
            break;
        }
    }
    if (pass) {
        spdlog::info("All values match within tolerance");
    }
    staging_mem.unmapMemory();
}

void Window::CreateGraphicsPipeline() {
    // 加载顶点和片段着色器
    vert_shader_module_ = LoadShader("shaders/fullscreen.vert.spv");
    frag_shader_module_ = LoadShader("shaders/fullscreen.frag.spv");

    // 着色器阶段
    vk::PipelineShaderStageCreateInfo vertexStage{};
    vertexStage.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(**vert_shader_module_)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(**frag_shader_module_)
        .setPName("main");

    std::array stages = {vertexStage, fragmentStage};

    // 顶点输入状态（无顶点属性）
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.setVertexBindingDescriptionCount(0)
        .setVertexAttributeDescriptionCount(0);

    // 输入装配状态
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
        .setPrimitiveRestartEnable(VK_FALSE);

    // 视口和剪刀（动态状态，管线创建时无需填充）
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1)
        .setScissorCount(1);

    // 光栅化状态
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setFrontFace(vk::FrontFace::eClockwise)
        .setLineWidth(1.0f);

    // 多重采样状态
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    // 颜色混合附件
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.setBlendEnable(VK_FALSE)
        .setColorWriteMask(vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);

    // 颜色混合状态
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.setAttachmentCount(1)
        .setPAttachments(&colorBlendAttachment);

    // 动态状态
    std::array dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    // 管线布局
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayoutCount(1)
        .setPSetLayouts(&**descriptor_set_layout_);

    graphics_pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        device_->createPipelineLayout(pipelineLayoutInfo)
    );

    // 图形管线创建信息
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStageCount(stages.size())
        .setPStages(stages.data())
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPColorBlendState(&colorBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(**graphics_pipeline_layout_)
        .setRenderPass(**render_pass_)
        .setSubpass(0);

    // 创建图形管线
    graphics_pipeline_ = std::make_unique<vk::raii::Pipeline>(
        device_->createGraphicsPipeline(nullptr, pipelineInfo)
    );
}
