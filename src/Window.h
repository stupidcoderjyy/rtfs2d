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

    void CreateVkInstance();
    std::vector<const char*> GetEnabledExtensions();
    std::vector<const char*> GetEnabledValidationLayers();
    vk::ApplicationInfo GetApplicationInfo() const;
    vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo() const;

    void CreateWindowSurface();
    void CheckPhysicalDevice();
    void CreateLogicalDevice();

};

}

#endif //RTFS2D_WINDOW_H
