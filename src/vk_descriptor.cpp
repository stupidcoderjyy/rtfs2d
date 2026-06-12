//
// Created by PC on 2026/6/9.
//

#include "vk_descriptor.h"

using namespace rtfs2d;

DescriptorSetBuilder::DescriptorSetBuilder(const vk::raii::Device& device)
    : device_(&device) {}

DescriptorSetBuilder& DescriptorSetBuilder::AddStorageBufferBinding(
        uint32_t binding, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding lb{};
    lb.setBinding(binding)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(stages);
    bindings_.push_back(lb);
    return *this;
}

DescriptorSetBuilder::BuildResult DescriptorSetBuilder::build(uint32_t max_sets) const {
    vk::DescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.setBindingCount(static_cast<uint32_t>(bindings_.size()))
        .setPBindings(bindings_.data());
    auto layout = std::make_unique<vk::raii::DescriptorSetLayout>(
        device_->createDescriptorSetLayout(layout_ci));

    vk::DescriptorPoolSize ps{};
    ps.setType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(static_cast<uint32_t>(bindings_.size() * max_sets));
    vk::DescriptorPoolCreateInfo pool_ci{};
    pool_ci.setPoolSizeCount(1)
        .setPPoolSizes(&ps)
        .setMaxSets(max_sets)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    auto pool = std::make_unique<vk::raii::DescriptorPool>(
        device_->createDescriptorPool(pool_ci));

    vk::DescriptorSetAllocateInfo ai{};
    std::vector layouts_vec(max_sets, **layout);
    ai.setDescriptorSetCount(max_sets)
        .setDescriptorPool(**pool)
        .setPSetLayouts(layouts_vec.data());
    auto sets = device_->allocateDescriptorSets(ai);
    std::vector<std::unique_ptr<vk::raii::DescriptorSet>> vec;
    vec.reserve(max_sets);
    for (int i = 0; i < max_sets; ++i) {
        vec.push_back(std::make_unique<vk::raii::DescriptorSet>(std::move(sets[i])));
    }
    return {std::move(layout), std::move(pool), std::move(vec)};
}

void DescriptorSetBuilder::WriteBuffer(const vk::raii::DescriptorSet& set,
        uint32_t binding,
        const vk::raii::Buffer& buffer) const {
    vk::DescriptorBufferInfo bi{};
    bi.setBuffer(*buffer)
        .setOffset(0)
        .setRange(VK_WHOLE_SIZE);
    vk::WriteDescriptorSet wds{};
    wds.setDstSet(*set)
        .setDstBinding(binding)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setPBufferInfo(&bi);
    device_->updateDescriptorSets(wds, nullptr);
}