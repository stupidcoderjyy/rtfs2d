//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_DEVICE_H
#define RTFS2D_VK_DEVICE_H

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

namespace rtfs2d {

class DeviceManager {
public:
    DeviceManager(GLFWwindow* window, bool debug_enabled);

    vk::raii::SurfaceKHR& surface() const {
        return *surface_;
    }

    vk::raii::PhysicalDevice& physical_device() {
        return physical_device_;
    }
    const vk::raii::Device& device() const {
        return *device_;
    }
    const vk::raii::Queue& graphics_queue() const {
        return *graphics_queue_;
    }
    const vk::raii::Queue& present_queue() const {
        return *present_queue_;
    }
    const vk::raii::CommandPool& command_pool() const {
        return *command_pool_;
    }
    const vk::raii::CommandPool& compute_command_pool() const {
        return *compute_command_pool_;
    }
    uint32_t graphics_queue_family_index() const {
        return graphics_queue_family_index_;
    }
    uint32_t present_queue_family_index() const {
        return present_queue_family_index_;
    }
private:
    std::unique_ptr<vk::raii::Instance> instance_;
    bool debug_enabled_;
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> debug_messenger_;
    std::unique_ptr<vk::raii::SurfaceKHR> surface_;
    vk::raii::PhysicalDevice physical_device_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_index_;
    uint32_t present_queue_family_index_;
    std::unique_ptr<vk::raii::Device> device_;
    std::unique_ptr<vk::raii::Queue> graphics_queue_;
    std::unique_ptr<vk::raii::Queue> present_queue_;
    std::unique_ptr<vk::raii::CommandPool> command_pool_;
    std::unique_ptr<vk::raii::CommandPool> compute_command_pool_;

    void CreateVkInstance();
    std::vector<const char*> GetEnabledExtensions();
    std::vector<const char*> GetEnabledValidationLayers();
    static vk::ApplicationInfo GetApplicationInfo();
    vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo() const;
    void CreateWindowSurface(GLFWwindow *window);
    void CheckPhysicalDevice();
    void CreateLogicalDevice();
    void CreateCommandPool();
    void CreateComputeCommandPool();
};

}


#endif //RTFS2D_VK_DEVICE_H
