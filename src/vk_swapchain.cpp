//
// Created by PC on 2026/6/9.
//

#include <spdlog/spdlog.h>

#include "vk_swapchain.h"
#include "vk_device.h"

using namespace rtfs2d;

SwapchainContext::SwapchainContext(DeviceManager &dm, int width, int height):
        device_manager_(&dm) {
    CreateSwapChain(width, height, false);
    CreateImageViews();
    CreateRenderPass();
    CreateFrameBuffers();
    CreateCommandBuffers();
    CreateFrameBasedSyncObjects();
    CreateImageBasedSyncObjects();
}

vk::Result SwapchainContext::Present(
        const vk::raii::Queue &queue,
        uint32_t img_idx,
        const vk::raii::Semaphore &wait_sem) {
    vk::PresentInfoKHR pi{};
    pi.setWaitSemaphores(*wait_sem)
        .setSwapchains(**swapchain_)
        .setImageIndices(img_idx);
    auto result = queue.presentKHR(pi);
    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR) {
        spdlog::warn("present returned: {}, swapchain needs recreate", static_cast<int>(result));
        needs_recreate_ = true;
    }
    return result;
}

void SwapchainContext::Recreate(int width, int height) {
    spdlog::warn("Recreate swapchain");
    device_manager_->device().waitIdle();
    framebuffers_.clear();
    swapchain_image_views_.clear();
    render_finished_semaphores_.clear();
    CreateSwapChain(true, width, height);
    CreateImageViews();
    CreateFrameBuffers();
    CreateImageBasedSyncObjects();
    needs_recreate_ = false;
}

void SwapchainContext::CreateSwapChain(int width, int height, bool replace) {
    vk::SwapchainCreateInfoKHR ci{};

    if (replace) {
        ci.setOldSwapchain(**swapchain_);
    }

    // 指定交换链渲染的目标表面
    ci.setSurface(*device_manager_->surface());

    // 查询 surface 的能力上限（最小/最大图像数量、当前窗口尺寸等）
    auto capabilities = device_manager_->physical_device().getSurfaceCapabilitiesKHR(*device_manager_->surface());
    vk::Extent2D ext = capabilities.currentExtent;  // 当前窗口尺寸
    // 若窗口系统不提供固定尺寸（currentExtent 为 UINT32_MAX），则用构造参数手动 clamp
    if (ext.width == UINT32_MAX) {
        ext.width = std::clamp(
            static_cast<uint32_t>(width),
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        ext.height = std::clamp(
            static_cast<uint32_t>(height),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }
    swapchain_extent_ = ext;
    ci.setImageExtent(swapchain_extent_);

    // 查询 surface 支持的像素格式和色彩空间，优先选择 sRGB
    auto formats = device_manager_->physical_device().getSurfaceFormatsKHR(*device_manager_->surface());
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
    if (auto modes = device_manager_->physical_device().getSurfacePresentModesKHR(*device_manager_->surface());
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
    if (device_manager_->graphics_queue_family_index() == device_manager_->present_queue_family_index()) {
        ci.setImageSharingMode(vk::SharingMode::eExclusive);
    } else {
        uint32_t indices[] = { device_manager_->graphics_queue_family_index(), device_manager_->present_queue_family_index() };
        ci.setImageSharingMode(vk::SharingMode::eConcurrent).setQueueFamilyIndices(indices);
    }

    // 创建 RAII 交换链，析构时自动销毁
    swapchain_ = std::make_unique<vk::raii::SwapchainKHR>(device_manager_->device(), ci);
    // 获取交换链中的图像数组
    swapchain_images_ = swapchain_->getImages();
    image_layouts_ = std::vector(swapchain_images_.size(), vk::ImageLayout::eUndefined);
}

void SwapchainContext::CreateImageViews() {
    for (const auto& img : swapchain_images_) {
        vk::ImageViewCreateInfo ci{};
        ci.setImage(img)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(swapchain_image_format_)
            .setSubresourceRange({
                vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
            });
        swapchain_image_views_.push_back(
            std::make_unique<vk::raii::ImageView>(device_manager_->device().createImageView(ci))
        );
    }
}

void SwapchainContext::CreateRenderPass() {
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
    render_pass_ = std::make_unique<vk::raii::RenderPass>(device_manager_->device().createRenderPass(ci));
}

void SwapchainContext::CreateFrameBuffers() {
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
            std::make_unique<vk::raii::Framebuffer>(device_manager_->device().createFramebuffer(ci))
        );
    }
}

void SwapchainContext::CreateCommandBuffers() {
    // 从命令池分配命令缓冲，数量与帧缓冲一致
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(device_manager_->command_pool())
        .setCommandBufferCount(kMaxFramesInFlight)             // 和“飞行帧”数量一致
        .setLevel(vk::CommandBufferLevel::ePrimary);           // 主命令缓冲
    command_buffers_ = device_manager_->device().allocateCommandBuffers(ai);
}

void SwapchainContext::CreateFrameBasedSyncObjects() {
    vk::FenceCreateInfo ci{};
    ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        image_available_semaphores_[i] =
            std::make_unique<vk::raii::Semaphore>(device_manager_->device().createSemaphore({}));
        in_flight_fences_[i] =
            std::make_unique<vk::raii::Fence>(device_manager_->device().createFence(ci));
    }
}

void SwapchainContext::CreateImageBasedSyncObjects() {
    for (int i = 0; i < swapchain_images_.size(); ++i) {
        render_finished_semaphores_.push_back(
            std::make_unique<vk::raii::Semaphore>(device_manager_->device().createSemaphore({}))
        );
    }
}