//
// Created by PC on 2026/6/5.
//

#include <fstream>
#include <spdlog/spdlog.h>
#include <memory>
#include <stdexcept>
#include <vector>

#include "window.h"
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
        swapchain_ctx_ = std::make_unique<SwapchainContext>(*device_manager_, width_, height_);
        compute_ctx_ = std::make_unique<ComputeContext>(*device_manager_);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* win, int w, int h) {
            auto* rtfs_win = static_cast<Window*>(glfwGetWindowUserPointer(win));
            rtfs_win->swapchain_ctx_->set_needs_recreate(true);
            rtfs_win->width_ = w;
            rtfs_win->height_ = h;
        });
        auto window_guard = std::shared_ptr<void>(nullptr, [this](...) {
            glfwDestroyWindow(window_);
        });

        // 创建波浪界面的图形管线
        CreateGraphicsPipeline();

        // 主循环
        while (!glfwWindowShouldClose(window_)) {
            if (swapchain_ctx_->needs_recreate()) {
                if (width_ == 0 || height_ == 0) {
                    glfwWaitEvents();   //最小化时阻塞当前线程，恢复窗口后再重建交换链
                } else {
                    swapchain_ctx_->Recreate(width_, height_);
                }
                continue;
            }
            auto& cb = swapchain_ctx_->command_buffers()[current_frame_];
            auto& fence = swapchain_ctx_->in_flight_fence(current_frame_);
            auto& acquire_sem = swapchain_ctx_->image_available_semaphore(current_frame_);

            // 当CPU运行速度过快，达到kMaxFramesInFlight的限制时，必须等待GPU完成该帧的渲染，才能进行下一次提交
            // 需要为每个飞行帧准备独立的fence
            // 当渲染速度较慢时，会在这里阻塞
            if (auto result = device_manager_->device().waitForFences(*fence, VK_TRUE, UINT64_MAX);
                    result != vk::Result::eSuccess) {
                throw std::runtime_error("waitForFences failed: " + std::to_string(static_cast<int>(result)));
            }
            device_manager_->device().resetFences(*fence);

            // 完成并验证着色器计算
            compute_ctx_->RecordAndSubmit(device_manager_->graphics_queue());

            // 尝试从交换链获取一张处于空闲状态图像的所有权
            // 一张图片可能有六个状态：空闲状态 → 被CPU占用 → 处于渲染队列 → 正在渲染 → 处于呈现队列 → 正在呈现
            // acquire_sem用于GPU端图形队列和呈现队列之间进行同步，避免对未完成呈现（只读）的图片进行渲染（写）
            // GPU端最多只有kMaxFramesInFlight张图片处于未完成渲染的状态，acquire_sem和kMaxFramesInFlight绑定
            // 当呈现速度较慢时，会在这里阻塞
            auto [res, img_idx] = swapchain_ctx_->AcquireImage(acquire_sem);
            if (res == vk::Result::eErrorOutOfDateKHR) {
                spdlog::warn("failed to acquire image from swapchain, try recreate swapchain");
                swapchain_ctx_->Recreate(width_, height_);
                continue;
            }

            // 重新录制命令
            RecordCommands(cb, img_idx);

            // 提交命令缓冲区。GPU会等待，直到图像就绪（acquire_sem被激活），CPU端会立刻返回，并准备下一帧的内容
            // 需要为每个飞行帧准备独立的acquire_sem
            // render_sem的作用是在GPU端图形队列和呈现队列之间进行同步，避免把未完成渲染的图片输出到屏幕
            // 需要为每张交换链图像准备一个独立的render_sem
            vk::PipelineStageFlags stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            auto& render_sem = swapchain_ctx_->render_finished_semaphore(img_idx);
            vk::SubmitInfo si{};
            si.setWaitSemaphores(*acquire_sem)
                .setWaitDstStageMask(stage_flags)
                .setSignalSemaphores(*render_sem)
                .setCommandBuffers(*cb);
            device_manager_->graphics_queue().submit(si, *fence);

            // 将图像加入呈现队列。呈现和渲染是GPU中两个独立的模块，每张图片基于render_sem进行同步
            if (auto pres_res = swapchain_ctx_->Present(device_manager_->present_queue(), img_idx, render_sem); pres_res != vk::Result::eSuccess) {
                spdlog::warn("present returned: {}", static_cast<int>(pres_res));
            }

            // 更新帧索引
            current_frame_ = (current_frame_ + 1) % 2;
            glfwPollEvents();
        }
        device_manager_->device().waitIdle();
    } catch (const std::exception& e) {
        spdlog::error("Show() failed: {}", e.what());
        device_manager_->device().waitIdle();
        throw;
    }
}

void Window::RecordCommands(const vk::raii::CommandBuffer &cb, uint32_t img_idx) const {
    cb.begin({});

    // 图像布局转换：从旧布局到颜色附件最优布局
    vk::ImageMemoryBarrier barrier{};
    vk::ImageSubresourceRange sr{};
    sr.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    barrier.setImage(swapchain_ctx_->swapchain_images()[img_idx])
        .setOldLayout(swapchain_ctx_->image_layouts()[img_idx])
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
    bi.setRenderPass(swapchain_ctx_->render_pass())
        .setFramebuffer(*swapchain_ctx_->framebuffers()[img_idx])
        .setRenderArea({{0, 0, swapchain_ctx_->extent().width, swapchain_ctx_->extent().height}})
        .setClearValueCount(1)
        .setPClearValues(&clear_value);
    cb.beginRenderPass(bi, vk::SubpassContents::eInline);

    // 设置动态视口和剪刀（必须与交换链尺寸一致）
    vk::Viewport viewport(0.0f, 0.0f,
        static_cast<float>(swapchain_ctx_->extent().width),
        static_cast<float>(swapchain_ctx_->extent().height),
        0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, swapchain_ctx_->extent());
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    // 绑定图形管线
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, **graphics_pipeline_);

    // 将存储缓冲绑定到图形管线
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        **graphics_pipeline_layout_, 0, *compute_ctx_->descriptor_set(), nullptr);

    // 绘制全屏三角形（3个顶点）
    cb.draw(3, 1, 0, 0);

    cb.endRenderPass();

    // 将图像布局转换为呈现源布局
    cb.end();
    swapchain_ctx_->image_layouts()[img_idx] = vk::ImageLayout::ePresentSrcKHR;
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

void Window::CreateGraphicsPipeline() {
    // 加载顶点和片段着色器
    auto vert_shader_module = LoadShader("shaders/fullscreen.vert.spv");
    auto frag_shader_module = LoadShader("shaders/fullscreen.frag.spv");

    // 着色器阶段
    vk::PipelineShaderStageCreateInfo vertexStage{};
    vertexStage.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(**vert_shader_module)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(**frag_shader_module)
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
        .setPSetLayouts(&*compute_ctx_->descriptor_set_layout());

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
        .setRenderPass(swapchain_ctx_->render_pass())
        .setSubpass(0);

    // 创建图形管线
    graphics_pipeline_ = std::make_unique<vk::raii::Pipeline>(
        device_manager_->device().createGraphicsPipeline(nullptr, pipelineInfo)
    );
}