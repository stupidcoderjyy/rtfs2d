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

    using BuildResult = std::tuple<
        std::unique_ptr<vk::raii::DescriptorSetLayout>,
        std::unique_ptr<vk::raii::DescriptorPool>,
        std::unique_ptr<vk::raii::DescriptorSet>>;

    BuildResult build() const;

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