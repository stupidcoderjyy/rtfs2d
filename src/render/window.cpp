//
// Created by PC on 2026/6/5.
//

#include <fstream>
#include <spdlog/spdlog.h>
#include <memory>
#include <stdexcept>

#include "window.h"
#include "imgui_control_panel.h"
#include "backends/imgui_impl_glfw.h"
#include "solver/case_data.h"
#include "vulkan/descriptor_sets.h"
#include "vulkan/buffers.h"

namespace rtfs2d {

Window::Window(std::unique_ptr<CaseData> case_data, bool debug_enabled):
        width_(), height_(), case_data_(std::move(case_data)), debug_enabled_(debug_enabled) {}

void Window::Show() {
    try {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        auto glfw_guard = std::shared_ptr<void>(nullptr, [this](...) {
            imgui_ctx_->Shutdown();
            glfwTerminate();
        });
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        std::string title = "rtfs2d - " + case_data_->name();
        width_ = case_data_->nx();
        height_ = case_data_->ny();
        window_ = glfwCreateWindow(width_, height_, title.c_str(), nullptr, nullptr);
        device_manager_ = std::make_unique<DeviceManager>(window_, debug_enabled_);
        swapchain_ctx_ = std::make_unique<SwapchainContext>(*device_manager_, width_, height_);
        buffers::InitBuffers(*device_manager_, *case_data_);
        DescriptorSets descriptor_sets(*device_manager_);
        compute_ctx_ = std::make_unique<ComputeContext>(*device_manager_, descriptor_sets,
            *case_data_, vis_config_);
        graphics_ctx_ = std::make_unique<GraphicsContext>(*device_manager_, *swapchain_ctx_,
            *compute_ctx_, descriptor_sets, *case_data_);
        imgui_ctx_ = std::make_unique<ImGuiVulkanContext>(window_,
            *device_manager_, *swapchain_ctx_->render_pass(),
            swapchain_ctx_->swapchain_images().size());
        ImGuiControlPanel imgui_control(vis_config_);
        //上传流程数据
        compute_ctx_->UploadCaseData();

        glfwSetWindowUserPointer(window_, this);
        glfwSetMouseButtonCallback(window_, MouseButtonCallback);
        glfwSetCursorPosCallback(window_, CursorPosCallback);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* win, int w, int h) {
            auto* rtfs_win = static_cast<Window*>(glfwGetWindowUserPointer(win));
            rtfs_win->swapchain_ctx_->set_needs_recreate(true);
            rtfs_win->width_ = w;
            rtfs_win->height_ = h;
        });
        auto window_guard = std::shared_ptr<void>(nullptr, [this](...) {
            glfwDestroyWindow(window_);
        });

        // 主循环
        while (!glfwWindowShouldClose(window_)) {
            if (swapchain_ctx_->needs_recreate()) {
                try {
                    if (width_ == 0 || height_ == 0) {
                        glfwWaitEvents();   //最小化时阻塞当前线程，恢复窗口后再重建交换链
                    } else {
                        swapchain_ctx_->Recreate(width_, height_);
                    }
                } catch (vk::OutOfDateKHRError&) {
                    glfwWaitEvents();
                }
                continue;
            }
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

            auto& cb = swapchain_ctx_->command_buffers()[current_frame_];
            cb.reset();
            cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            // 完成并验证着色器计算
            if (!vis_config_.paused) {
                compute_ctx_->RecordCommands(cb);
            }

            // 尝试从交换链获取一张处于空闲状态图像的所有权
            // 一张图片可能有六个状态：空闲状态 → 被CPU占用 → 处于渲染队列 → 正在渲染 → 处于呈现队列 → 正在呈现
            // acquire_sem用于GPU端图形队列和呈现队列之间进行同步，避免对未完成呈现（只读）的图片进行渲染（写）
            // GPU端最多只有kMaxFramesInFlight张图片处于未完成渲染的状态，acquire_sem和kMaxFramesInFlight绑定
            // 当呈现速度较慢时，会在这里阻塞
            uint32_t img_idx;
            try {
                img_idx = swapchain_ctx_->AcquireImage(acquire_sem).value;
            } catch ([[maybe_unused]] const vk::OutOfDateKHRError& e) {
                swapchain_ctx_->set_needs_recreate(true);
                continue;
            }

            // 开始渲染
            graphics_ctx_->BeginRenderPass(cb, img_idx);

            // 流场渲染
            graphics_ctx_->RecordCommands(cb, vis_config_);

            // GUI渲染
            imgui_ctx_->BeginFrame();
            imgui_control.Render();
            imgui_ctx_->EndFrame(cb);

            // 渲染结束
            graphics_ctx_->EndRenderPass(cb, img_idx);
            cb.end();

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
            try {
                swapchain_ctx_->Present(device_manager_->present_queue(), img_idx, render_sem);
            } catch (const vk::OutOfDateKHRError& e) {
                spdlog::warn("present out of date", e.what());
                swapchain_ctx_->set_needs_recreate(true);
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

void Window::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    auto* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            w->compute_ctx_->SetDyeInjecting(true);
        } else if (action == GLFW_RELEASE) {
            w->compute_ctx_->SetDyeInjecting(false);
        }
    }
}

void Window::CursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    auto* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
    float nx = static_cast<float>(xpos) / static_cast<float>(w->width_);
    float ny = static_cast<float>(ypos) / static_cast<float>(w->height_); // 翻转 Y 轴
    w->compute_ctx_->SetDyeInjectPos(nx, ny);
}

}  // namespace rtfs2d
