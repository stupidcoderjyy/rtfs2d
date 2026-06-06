//
// Created by PC on 2026/6/5.
//

#ifndef RTFS2D_WINDOW_H
#define RTFS2D_WINDOW_H
#include <string>
#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_NONE
#include <functional>
#include <GLFW/glfw3.h>

namespace rtfs2d {

class Window {
public:
    Window(int width, int height, std::string title, bool debug_enabled = true);
    void Show();
private:
    int width_, height_;
    std::string title_;
    GLFWwindow* window_{};
    std::unique_ptr<vk::raii::Instance> instance_;
    bool debug_enabled_;
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> debug_messenger_;
    std::unique_ptr<vk::raii::SurfaceKHR> surface_;
    vk::raii::PhysicalDevice physical_device_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_index_{};
    uint32_t present_queue_family_index_{};
    std::unique_ptr<vk::raii::Device> device_;
    std::unique_ptr<vk::raii::Queue> graphics_queue_;
    std::unique_ptr<vk::raii::Queue> present_queue_;
    std::unique_ptr<vk::raii::SwapchainKHR> swapchain_;
    vk::Format swapchain_image_format_{};
    vk::ColorSpaceKHR swapchain_color_space_{};
    vk::Extent2D swapchain_extent_;
    std::vector<vk::Image> swapchain_images_;
    std::vector<std::unique_ptr<vk::raii::ImageView>> swapchain_image_views_;
    std::unique_ptr<vk::raii::RenderPass> render_pass_;
    std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers_;
    std::unique_ptr<vk::raii::CommandPool> command_pool_;
    std::vector<vk::raii::CommandBuffer> command_buffers_;
    //限制同时处于飞行状态的帧数，避免 CPU 超前 GPU 过多
    static constexpr uint32_t kMaxFramesInFlight = 2;
    std::array<std::unique_ptr<vk::raii::Semaphore>, kMaxFramesInFlight> image_available_semaphores_;
    std::vector<std::unique_ptr<vk::raii::Semaphore>> render_finished_semaphores_;
    std::array<std::unique_ptr<vk::raii::Fence>, kMaxFramesInFlight> in_flight_fences_;
    size_t current_frame_ = 0;
    std::vector<vk::ImageLayout> image_layouts_;
    bool frame_buffer_resized_ = false;

    void CreateVkInstance();
    std::vector<const char*> GetEnabledExtensions();
    std::vector<const char*> GetEnabledValidationLayers();
    vk::ApplicationInfo GetApplicationInfo() const;
    vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo() const;

    void CreateWindowSurface();
    void CheckPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapChain(bool replace = false);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFrameBuffers();
    void CreateCommandPoolAndBuffers();
    void CreateFrameBasedSyncObjects();
    void CreateImageBasedSyncObjects();
    void RecordCommands(const vk::raii::CommandBuffer& cb, uint32_t img_idx);
    void RecreateSwapChain();
};

}

#endif //RTFS2D_WINDOW_H
