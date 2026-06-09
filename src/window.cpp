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

#include "vk_memory.h"

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
        window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
        device_manager_ = std::make_unique<DeviceManager>(window_, debug_enabled_);
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
        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateFrameBuffers();
        CreateCommandBuffers();
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
            if (auto result = device_manager_->device().waitForFences(**in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
                    result != vk::Result::eSuccess) {
                throw std::runtime_error("waitForFences failed: " + std::to_string(static_cast<int>(result)));
            }
            device_manager_->device().resetFences(**fence);

            // 完成并验证着色器计算
            vk::SubmitInfo c_si{};
            c_si.setCommandBuffers(**compute_command_buffer_);
            device_manager_->graphics_queue().submit(c_si);
            device_manager_->graphics_queue().waitIdle();
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
            device_manager_->graphics_queue().submit(si, **fence);

            // 将图像加入呈现队列。呈现和渲染是GPU中两个独立的模块，每张图片基于render_sem进行同步
            vk::PresentInfoKHR pi{};
            pi.setWaitSemaphores(**render_sem)
                .setSwapchains(**swapchain_)
                .setImageIndices(img_idx);
            if (auto result = device_manager_->present_queue().presentKHR(pi);
                    result == vk::Result::eErrorOutOfDateKHR ||
                    result == vk::Result::eSuboptimalKHR) {
                spdlog::warn("failed to present image: {}, try recreate swapchain", static_cast<int>(result));
                frame_buffer_resized_ = true;
            }

            // 更新帧索引
            current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
            glfwPollEvents();
        }
        device_manager_->device().waitIdle();
    } catch (const std::exception& e) {
        spdlog::error("Show() failed: {}", e.what());
        device_manager_->device().waitIdle();
        throw;
    }
}

void Window::CreateSwapChain(bool replace) {
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
            std::make_unique<vk::raii::ImageView>(device_manager_->device().createImageView(ci))
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
    render_pass_ = std::make_unique<vk::raii::RenderPass>(device_manager_->device().createRenderPass(ci));
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
            std::make_unique<vk::raii::Framebuffer>(device_manager_->device().createFramebuffer(ci))
        );
    }
}

void Window::CreateCommandBuffers() {
    // 从命令池分配命令缓冲，数量与帧缓冲一致
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(device_manager_->command_pool())
        .setCommandBufferCount(kMaxFramesInFlight)             // 和“飞行帧”数量一致
        .setLevel(vk::CommandBufferLevel::ePrimary);           // 主命令缓冲
    command_buffers_ = device_manager_->device().allocateCommandBuffers(ai);
}

void Window::CreateFrameBasedSyncObjects() {
    vk::FenceCreateInfo ci{};
    ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        image_available_semaphores_[i] =
            std::make_unique<vk::raii::Semaphore>(device_manager_->device().createSemaphore({}));
        in_flight_fences_[i] =
            std::make_unique<vk::raii::Fence>(device_manager_->device().createFence(ci));
    }
}

void Window::CreateImageBasedSyncObjects() {
    for (int i = 0; i < swapchain_images_.size(); ++i) {
        render_finished_semaphores_.push_back(
            std::make_unique<vk::raii::Semaphore>(device_manager_->device().createSemaphore({}))
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
    device_manager_->device().waitIdle();
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
    return std::make_unique<vk::raii::ShaderModule>(device_manager_->device().createShaderModule(ci));
}

void Window::CreateStorageBuffer() {
    auto [buf, mem] = AllocateBuffer(device_manager_->device(), device_manager_->physical_device(), compute_buf_size_,
        vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    scalar_field_buffer_ = std::move(buf);
    scalar_field_memory_ = std::move(mem);
    std::vector host_data(compute_cell_count_, 0.0f);
    UploadBufferData(device_manager_->device(), device_manager_->physical_device(), device_manager_->command_pool(), device_manager_->graphics_queue(), host_data, *scalar_field_buffer_);
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
        device_manager_->device().createDescriptorSetLayout(ci));
}

void Window::CreatePipelineLayout() {
    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&**descriptor_set_layout_);
    pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        device_manager_->device().createPipelineLayout(ci));
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
        device_manager_->device().createComputePipeline(nullptr, cp_ci));
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
        device_manager_->device().createDescriptorPool(ci));
}

void Window::CreateDescriptorSet() {
    vk::DescriptorSetAllocateInfo ai{};
    ai.setDescriptorSetCount(1)
        .setDescriptorPool(**descriptor_pool_)
        .setPSetLayouts(&**descriptor_set_layout_);
    auto ds = device_manager_->device().allocateDescriptorSets(ai);
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
    device_manager_->device().updateDescriptorSets(wds, nullptr);
}


void Window::RecordComputeCommands() {
    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(device_manager_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = device_manager_->device().allocateCommandBuffers(ai);
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
    auto staging_buf = device_manager_->device().createBuffer(ci);

    auto mr = staging_buf.getMemoryRequirements();
    auto mti = FindMemoryType(device_manager_->physical_device(), mr.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo m_ai{};
    m_ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    auto staging_mem = device_manager_->device().allocateMemory(m_ai);
    staging_buf.bindMemory(*staging_mem, 0);

    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(device_manager_->compute_command_pool())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    auto cbs = device_manager_->device().allocateCommandBuffers(ai);
    auto& cb = cbs.front();
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::BufferCopy copy{};
    copy.setSrcOffset(0)
        .setSize(compute_buf_size_);
    cb.copyBuffer(**scalar_field_buffer_, *staging_buf, copy);
    cb.end();

    vk::SubmitInfo si{};
    si.setCommandBuffers(*cb);
    device_manager_->graphics_queue().submit(si);
    device_manager_->graphics_queue().waitIdle();

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
        device_manager_->device().createPipelineLayout(pipelineLayoutInfo)
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
        device_manager_->device().createGraphicsPipeline(nullptr, pipelineInfo)
    );
}
