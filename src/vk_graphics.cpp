//
// Created by PC on 2026/6/9.
//

#include "vk_graphics.h"

#include <array>
#include <fstream>
#include <spdlog/spdlog.h>
#include <vector>

#include "descriptor_sets.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_compute.h"

namespace rtfs2d {

GraphicsContext::GraphicsContext(DeviceManager& dm, SwapchainContext& sc, ComputeContext& cc, DescriptorSets& ds) :
        dm_(&dm), sc_(&sc), cc_(&cc), ds_(&ds) {
    CreateGraphicsPipeline();
}

void GraphicsContext::RecordCommands(const vk::raii::CommandBuffer& cb, uint32_t img_idx,
        uint32_t gradient_type, uint32_t vis_mode) const {
    // 图像布局转换：从旧布局到颜色附件最优布局
    vk::ImageMemoryBarrier barrier{};
    vk::ImageSubresourceRange sr{};
    sr.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    barrier.setImage(sc_->swapchain_images()[img_idx])
        .setOldLayout(sc_->image_layouts()[img_idx])
        .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setSrcAccessMask(vk::AccessFlagBits::eNone)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
        .setSubresourceRange(sr)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, nullptr, nullptr, barrier);

    // 开始渲染通道，清除颜色为黑色
    vk::ClearValue clear_value{std::array{0.0f, 0.0f, 0.0f, 1.0f}};
    vk::RenderPassBeginInfo bi{};
    bi.setRenderPass(sc_->render_pass())
        .setFramebuffer(*sc_->framebuffers()[img_idx])
        .setRenderArea({{0, 0, sc_->extent().width, sc_->extent().height}})
        .setClearValueCount(1)
        .setPClearValues(&clear_value);
    cb.beginRenderPass(bi, vk::SubpassContents::eInline);

    // 设置动态视口和剪刀（必须与交换链尺寸一致）
    vk::Viewport viewport(0.0f, 0.0f,
        static_cast<float>(sc_->extent().width),
        static_cast<float>(sc_->extent().height),
        0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, sc_->extent());
    cb.setViewport(0, viewport);
    cb.setScissor(0, scissor);

    // 绑定图形管线
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, **graphics_pipeline_);

    // 将存储缓冲绑定到图形管线
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        **graphics_pipeline_layout_, 0,
        *ds_->SetAt(cc_->GetVisDescriptorSetIndex()), nullptr);

    cb.pushConstants<uint32_t>(**graphics_pipeline_layout_,
        vk::ShaderStageFlagBits::eFragment, 0, {gradient_type, vis_mode});

    // 绘制全屏三角形（3个顶点）
    cb.draw(3, 1, 0, 0);

    cb.endRenderPass();

    // 将图像布局转换为呈现源布局
    sc_->image_layouts()[img_idx] = vk::ImageLayout::ePresentSrcKHR;
}

void GraphicsContext::CreateGraphicsPipeline() {
    // 加载顶点和片段着色器
    auto vert_shader_module = dm_->LoadShader("shaders/fullscreen.vert.spv");
    auto frag_shader_module = dm_->LoadShader("shaders/fullscreen.frag.spv");

    // 着色器阶段
    vk::PipelineShaderStageCreateInfo vertexStage{};
    vertexStage.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(**vert_shader_module)
        .setPName("main");


    const auto& params = cc_->grid_params();
    std::vector<vk::SpecializationMapEntry> mes = {
        {0, 0, sizeof(uint32_t)},   // constant_id 0 -> NX
        {1, sizeof(uint32_t), sizeof(uint32_t)},  // constant_id 1 -> NY
    };
    std::vector<uint8_t> bytes;
    bytes.reserve(2 * sizeof(uint32_t));
    AppendDataToBytesVec<uint32_t>(bytes, params.nx);
    AppendDataToBytesVec<uint32_t>(bytes, params.ny);

    vk::SpecializationInfo specInfo{};
    specInfo.setMapEntries(mes)
        .setData<uint8_t>(bytes);

    vk::PipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(**frag_shader_module)
        .setPName("main")
        .setPSpecializationInfo(&specInfo);

    std::array stages = {vertexStage, fragmentStage};

    // 顶点输入状态（无顶点属性）
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.setVertexBindingDescriptionCount(0)
        .setVertexAttributeDescriptionCount(0);

    // 输入装配状态
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
        .setPrimitiveRestartEnable(VK_FALSE);

    // 视口和剪刀（动态状态，管线创建时无需填充）
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1)
        .setScissorCount(1);

    // 光栅化状态
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setFrontFace(vk::FrontFace::eClockwise)
        .setLineWidth(1.0f);

    // 多重采样状态
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    // 颜色混合附件
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.setBlendEnable(VK_FALSE)
        .setColorWriteMask(vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);

    // 颜色混合状态
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.setAttachmentCount(1)
        .setPAttachments(&colorBlendAttachment);

    // 动态状态
    std::array dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    vk::PushConstantRange pcr{};
    pcr.setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setOffset(0)
        .setSize(2 * sizeof(uint32_t));

    // 管线布局
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayoutCount(1)
        .setPSetLayouts(&*ds_->descriptor_set_layout())
        .setPushConstantRanges(pcr);

    graphics_pipeline_layout_ = std::make_unique<vk::raii::PipelineLayout>(
        dm_->device().createPipelineLayout(pipelineLayoutInfo)
    );

    // 图形管线创建信息
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStageCount(stages.size())
        .setPStages(stages.data())
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPColorBlendState(&colorBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(**graphics_pipeline_layout_)
        .setRenderPass(sc_->render_pass())
        .setSubpass(0);

    // 创建图形管线
    graphics_pipeline_ = std::make_unique<vk::raii::Pipeline>(
        dm_->device().createGraphicsPipeline(nullptr, pipelineInfo)
    );
}

}  // namespace rtfs2d
