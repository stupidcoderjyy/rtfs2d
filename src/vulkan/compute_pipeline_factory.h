//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_COMPUTE_PIPELINE_FACTORY_H
#define RTFS2D_COMPUTE_PIPELINE_FACTORY_H
#include <memory>
#include <vulkan/vulkan_raii.hpp>

#include "vk_memory.h"

namespace rtfs2d {

class DeviceManager;
class ComputeContext;
class DescriptorSets;

class ComputeShaderTask {
    friend class ComputeShaderTaskBuilder;
public:
    ComputeShaderTask(std::unique_ptr<vk::raii::Pipeline> pipeline_, const std::shared_ptr<vk::raii::PipelineLayout> &layout_);

    ComputeShaderTask& Begin(const vk::CommandBuffer& cb, const vk::raii::DescriptorSet &ds) {
        cb_ = &cb;
        cb.bindPipeline(vk::PipelineBindPoint::eCompute, **pipeline_);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *layout_,0, *ds, nullptr);
        return *this;
    }

    template<typename T>
    ComputeShaderTask& PushConstant(vk::ArrayProxy<const T> const & values, uint32_t offset = 0) {
        cb_->pushConstants<T>(**layout_, vk::ShaderStageFlagBits::eCompute, offset, values);
        return *this;
    }

    void End(uint32_t unit_size) {
        cb_->dispatch((unit_size + 127) / 128, 1, 1);
        cb_ = nullptr;
    }
private:
    std::unique_ptr<vk::raii::Pipeline> pipeline_;
    std::shared_ptr<vk::raii::PipelineLayout> layout_;
    const vk::CommandBuffer* cb_;
};

namespace internal {

class ComputeShaderTaskBuilder {
    friend class ComputeShaderTaskFactory;
public:
    explicit ComputeShaderTaskBuilder(DeviceManager* dm, ComputeContext* cc, DescriptorSets* ds, std::string shader);
    template<typename T>
    ComputeShaderTaskBuilder& AppendSpecializationConst(const std::initializer_list<T>& spec) {
        for (const auto& val : spec) {
            auto offset = static_cast<uint32_t>(spec_const_bytes_.size());
            auto id = spec_const_size_++;
            spec_const_entries_.emplace_back(id, offset, sizeof(T));
            AppendDataToBytesVec<T>(spec_const_bytes_, val);
        }
        return *this;
    }
    ComputeShaderTaskBuilder& SetPipelineLayout(const std::shared_ptr<vk::raii::PipelineLayout>& layout);
    ComputeShaderTaskBuilder& SetPushConstSize(uint32_t size);
    std::unique_ptr<ComputeShaderTask> Build();
private:
    DeviceManager* dm_;
    ComputeContext* cc_;
    DescriptorSets* ds_;
    std::string shader_path_;
    std::vector<uint8_t> spec_const_bytes_;
    uint32_t spec_const_size_ = 0;
    uint32_t push_const_size_ = 0;
    std::vector<vk::SpecializationMapEntry> spec_const_entries_;
    std::shared_ptr<vk::raii::PipelineLayout> layout_;
    void CreatePipelineLayout();
};

}

class ComputeShaderTaskFactory {
public:
    ComputeShaderTaskFactory(DeviceManager& dm, ComputeContext& cc, DescriptorSets& ds);
    internal::ComputeShaderTaskBuilder Create(const std::string& shader_path) const;

private:
    DeviceManager* dm_;
    ComputeContext* cc_;
    DescriptorSets* ds_;
};

}

#endif //RTFS2D_COMPUTE_PIPELINE_FACTORY_H
