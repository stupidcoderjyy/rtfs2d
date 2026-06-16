//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_SWAPCHAIN_H
#define RTFS2D_VK_SWAPCHAIN_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class DeviceManager;

class SwapchainContext {
public:
    SwapchainContext(DeviceManager& dm, int width, int height);

    vk::Result Present(
        const vk::raii::Queue& queue,
        uint32_t img_idx,
        const vk::raii::Semaphore& wait_sem);

    void Recreate(int width, int height);

    vk::Extent2D extent() const {
        return swapchain_extent_;
    }

    vk::Format image_format() const {
        return swapchain_image_format_;
    }

    const std::vector<vk::Image>& swapchain_images() const {
        return swapchain_images_;
    }

    const vk::raii::RenderPass& render_pass() const {
        return *render_pass_;
    }

    const std::vector<std::unique_ptr<vk::raii::Framebuffer>>& framebuffers() const {
        return framebuffers_;
    }

    const std::vector<vk::raii::CommandBuffer>& command_buffers() const {
        return command_buffers_;
    }

    vk::raii::Fence& in_flight_fence(size_t frame) const {
        return *in_flight_fences_[frame];
    }

    vk::raii::Semaphore& image_available_semaphore(size_t frame) const {
        return *image_available_semaphores_[frame];
    }

    vk::raii::Semaphore& render_finished_semaphore(uint32_t img_idx) const {
        return *render_finished_semaphores_[img_idx];
    }

    std::vector<vk::ImageLayout>& image_layouts() {
        return image_layouts_;
    }

    vk::ResultValue<uint32_t> AcquireImage(const vk::raii::Semaphore& sem) const {
        return swapchain_->acquireNextImage(UINT64_MAX,*sem);
    }

    bool needs_recreate() const {
        return needs_recreate_;
    }

    void set_needs_recreate(bool b) {
        needs_recreate_ = b;
    }

private:
    DeviceManager* device_manager_;
    std::unique_ptr<vk::raii::SwapchainKHR> swapchain_;
    vk::Format swapchain_image_format_{};
    vk::ColorSpaceKHR swapchain_color_space_{};
    vk::Extent2D swapchain_extent_;
    std::vector<vk::Image> swapchain_images_;
    std::vector<std::unique_ptr<vk::raii::ImageView>> swapchain_image_views_;
    std::unique_ptr<vk::raii::RenderPass> render_pass_;
    std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers_;
    std::vector<vk::raii::CommandBuffer> command_buffers_;
    //限制同时处于飞行状态的帧数，避免 CPU 超前 GPU 过多
    static constexpr uint32_t kMaxFramesInFlight = 2;
    std::array<std::unique_ptr<vk::raii::Semaphore>, kMaxFramesInFlight> image_available_semaphores_;
    std::vector<std::unique_ptr<vk::raii::Semaphore>> render_finished_semaphores_;
    std::array<std::unique_ptr<vk::raii::Fence>, kMaxFramesInFlight> in_flight_fences_;
    std::vector<vk::ImageLayout> image_layouts_;
    bool needs_recreate_ = false;

    void CreateSwapChain(int width, int height, bool replace = false);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFrameBuffers();
    void CreateCommandBuffers();
    void CreateFrameBasedSyncObjects();
    void CreateImageBasedSyncObjects();
};

}


#endif //RTFS2D_VK_SWAPCHAIN_H
