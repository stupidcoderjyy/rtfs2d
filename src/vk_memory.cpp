//
// Created by PC on 2026/6/9.
//

#include "vk_memory.h"

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

void rtfs2d::UploadBufferData(
        const vk::raii::Device &device,
        vk::PhysicalDevice physical_device,
        const vk::raii::CommandPool &command_pool,
        const vk::raii::Queue &queue,
        const std::vector<float> &data,
        const vk::raii::Buffer &dst_buffer) {
    auto size = data.size() * sizeof(float);
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
