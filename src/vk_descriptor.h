//
// Created by PC on 2026/6/9.
//

#ifndef RTFS2D_VK_DESCRIPTOR_H
#define RTFS2D_VK_DESCRIPTOR_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

class DescriptorSetBuilder {
public:
    explicit DescriptorSetBuilder(const vk::raii::Device& device);

    DescriptorSetBuilder& AddStorageBufferBinding(uint32_t binding, vk::ShaderStageFlags stages);

    struct BuildResult {
        std::unique_ptr<vk::raii::DescriptorSetLayout> layout;
        std::unique_ptr<vk::raii::DescriptorPool> pool;
        std::vector<std::unique_ptr<vk::raii::DescriptorSet>> sets;
    };

    BuildResult build(uint32_t max_sets = 1) const;

    void WriteBuffer(
        const vk::raii::DescriptorSet& set,
        uint32_t binding,
        const vk::raii::Buffer& buffer) const;

private:
    const vk::raii::Device* device_;
    std::vector<vk::DescriptorSetLayoutBinding> bindings_;
};

}

#endif //RTFS2D_VK_DESCRIPTOR_H