//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_MEMORY_H
#define RTFS2D_VK_MEMORY_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

uint32_t FindMemoryType(
        vk::PhysicalDevice physical_device,
        uint32_t typeFilter,
        vk::MemoryPropertyFlags properties);

using BufferAndMemory =
        std::pair<std::unique_ptr<vk::raii::Buffer>, std::unique_ptr<vk::raii::DeviceMemory>>;

BufferAndMemory AllocateBuffer(
        const vk::raii::Device& device,
        vk::PhysicalDevice physical_device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags memory_flags);
}

#endif //RTFS2D_VK_MEMORY_H
