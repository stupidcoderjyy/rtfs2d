//
// Created by PC on 2026/6/9.
//

#include "vk_memory.h"


namespace rtfs2d {

uint32_t rtfs2d::FindMemoryType(
        vk::PhysicalDevice physical_device,
        uint32_t typeFilter,
        vk::MemoryPropertyFlags properties) {
    auto mp = physical_device.getMemoryProperties();
    for (int i = 0; i < mp.memoryTypes.size(); ++i) {
        if (typeFilter & 1 << i && properties == mp.memoryTypes[i].propertyFlags) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

rtfs2d::BufferAndMemory rtfs2d::AllocateBuffer(
        const vk::raii::Device &device,
        vk::PhysicalDevice physical_device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags memory_flags) {
    vk::BufferCreateInfo ci{};
    ci.setSize(size)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive);
    auto buffer = std::make_unique<vk::raii::Buffer>(device.createBuffer(ci));

    vk::DeviceBufferMemoryRequirements bmr{};
    bmr.setPCreateInfo(&ci);
    auto mr = device.getBufferMemoryRequirements(bmr).memoryRequirements;
    auto mti = FindMemoryType(physical_device, mr.memoryTypeBits, memory_flags);

    vk::MemoryAllocateInfo ai;
    ai.setAllocationSize(mr.size)
        .setMemoryTypeIndex(mti);
    auto memory = std::make_unique<vk::raii::DeviceMemory>(device.allocateMemory(ai));
    buffer->bindMemory(*memory, 0);
    return {std::move(buffer), std::move(memory)};
}

}  // namespace rtfs2d
