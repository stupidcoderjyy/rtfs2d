//
// Created by PC on 2026/6/15.
//

#include "compute_pipeline_factory.h"

#include <spdlog/spdlog.h>

#include "descriptor_sets.h"
#include "compute_context.h"
#include "device_manager.h"

namespace rtfs2d {

ComputeShaderTask::ComputeShaderTask(
        std::unique_ptr<vk::raii::Pipeline> pipeline_,
        const std::shared_ptr<vk::raii::PipelineLayout> &layout_)
        : pipeline_(std::move(pipeline_)), layout_(layout_), cb_() {
}

namespace internal {

ComputeShaderTaskBuilder::ComputeShaderTaskBuilder(
        DeviceManager* dm, ComputeContext* cc, DescriptorSets* ds, std::string shader)
        : dm_(dm), cc_(cc), ds_(ds), shader_path_(std::move(shader)) {
}

ComputeShaderTaskBuilder& ComputeShaderTaskBuilder::SetPipelineLayout(const std::shared_ptr<vk::raii::PipelineLayout>& layout) {
    layout_ = layout;
    return *this;
}

ComputeShaderTaskBuilder& ComputeShaderTaskBuilder::SetPushConstSize(uint32_t size) {
    push_const_size_ = size;
    return *this;
}

std::unique_ptr<ComputeShaderTask> ComputeShaderTaskBuilder::Build() {
    auto shader = dm_->LoadShader(shader_path_);
    vk::PipelineShaderStageCreateInfo pss_ci{};
    pss_ci.setStage(vk::ShaderStageFlagBits::eCompute)
        .setModule(**shader)
        .setPName("main");

    vk::SpecializationInfo specInfo{};
    if (!spec_const_entries_.empty()) {
        specInfo.setMapEntries(spec_const_entries_)
            .setData<uint8_t>(spec_const_bytes_);
        pss_ci.setPSpecializationInfo(&specInfo);
    }
    CreatePipelineLayout();
    vk::ComputePipelineCreateInfo cp_ci{};
    cp_ci.setStage(pss_ci)
        .setLayout(*layout_);
    auto pipeline = std::make_unique<vk::raii::Pipeline>(
        dm_->device().createComputePipeline(nullptr, cp_ci));

    return std::make_unique<ComputeShaderTask>(std::move(pipeline), layout_);
}

void ComputeShaderTaskBuilder::CreatePipelineLayout() {
    if (push_const_size_ == 0) {
        // 默认
        vk::PipelineLayoutCreateInfo ci{};
        ci.setSetLayoutCount(1)
            .setPSetLayouts(&*ds_->descriptor_set_layout());
        layout_ = std::make_shared<vk::raii::PipelineLayout>(
            dm_->device().createPipelineLayout(ci));
        return;
    }
    vk::PipelineLayoutCreateInfo ci{};
    vk::PushConstantRange pcr{};
    pcr.setStageFlags(vk::ShaderStageFlagBits::eCompute)
        .setOffset(0)
        .setSize(push_const_size_);
    ci.setSetLayoutCount(1)
        .setPSetLayouts(&*ds_->descriptor_set_layout())
        .setPushConstantRanges(pcr);
    layout_ = std::make_shared<vk::raii::PipelineLayout>(dm_->device().createPipelineLayout(ci));
}

}

ComputeShaderTaskFactory::ComputeShaderTaskFactory(DeviceManager& dm, ComputeContext& cc, DescriptorSets& ds):
        dm_(&dm), cc_(&cc), ds_(&ds){
}

internal::ComputeShaderTaskBuilder ComputeShaderTaskFactory::Create(const std::string &shader_path) const {
    internal::ComputeShaderTaskBuilder builder(dm_, cc_, ds_, shader_path);
    builder.SetPipelineLayout(cc_->pipeline_layout());
    return builder;
}

}
