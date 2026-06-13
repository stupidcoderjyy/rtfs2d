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

template<typename T>
void UploadBufferData(
        const vk::raii::Device &device,
        vk::PhysicalDevice physical_device,
        const vk::raii::CommandPool &command_pool,
        const vk::raii::Queue &queue,
        const std::vector<T> &data,
        const vk::raii::Buffer &dst_buffer) {
    auto size = data.size() * sizeof(T);
    auto [staging_buf, staging_mem] = AllocateBuffer(device, physical_device, size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* mp = staging_mem->mapMemory(0, size);
    std::memcpy(mp, data.data(), size);
    staging_mem->unmapMemory();

    vk::CommandBufferAllocateInfo ai{};
    ai.setCommandPool(*command_pool)
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    std::vector<vk::raii::CommandBuffer> cbs = device.allocateCommandBuffers(ai);
    auto& cb = cbs.front();

    // 开始命令缓冲区录制（OneTimeSubmit 标记优化）
    cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // 构造拷贝区域：全缓冲区拷贝，偏移为 0
    vk::BufferCopy copy_region;
    copy_region.setSrcOffset(0)
        .setDstOffset(0)
        .setSize(size);

    cb.copyBuffer(**staging_buf, *dst_buffer, copy_region);
    cb.end();

    vk::SubmitInfo si;
    si.setCommandBuffers(*cb);

    queue.submit(si);
    queue.waitIdle();
}

template<typename T>
void InsertDataToBytesVec(
        std::vector<uint8_t>& vec,
        const std::vector<uint8_t>::iterator& pos,
        const T& data) {
    auto* bytes = reinterpret_cast<const uint8_t*>(&data);
    vec.insert(pos, bytes, bytes + sizeof(T));
}

template<typename T>
void AppendDataToBytesVec(
        std::vector<uint8_t>& vec,
        const T& data) {
    auto* bytes = reinterpret_cast<const uint8_t*>(&data);
    vec.insert(vec.end(), bytes, bytes + sizeof(T));
}

}

#endif //RTFS2D_VK_MEMORY_H
