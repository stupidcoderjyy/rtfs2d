//
// Created by PC on 2026/6/16.
//

#include "imgui_vulkan_context.h"

#include <spdlog/spdlog.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "vulkan/device_manager.h"

namespace rtfs2d {

ImGuiVulkanContext::ImGuiVulkanContext(
        GLFWwindow* window,
        DeviceManager& dm,
        vk::RenderPass render_pass,
        uint32_t swapchain_image_count) {
    // 1. 创建 ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // 不使用 ini 文件
    ImGui::StyleColorsDark();

    // 2. 绑定 GLFW 后端
    ImGui_ImplGlfw_InitForVulkan(window, false);

    // 3. 创建 ImGui 专用的 Vulkan 描述符池
    constexpr uint32_t pool_count = 16;  // 可根据需要增大
    std::array pool_sizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, pool_count},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, pool_count}
    };
    vk::DescriptorPoolCreateInfo pool_ci{};
    pool_ci.setMaxSets(pool_count * 2)   // 至少为各类型计数之和
        .setPoolSizeCount(pool_sizes.size())
        .setPPoolSizes(pool_sizes.data())
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    imgui_descriptor_pool_ = std::make_unique<vk::raii::DescriptorPool>(
        dm.device().createDescriptorPool(pool_ci));

    // 4. 构造 ImGui Vulkan 初始化信息
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = *dm.vk_instance();
    init_info.PhysicalDevice = *dm.physical_device();
    init_info.Device = *dm.device();
    init_info.QueueFamily = dm.graphics_queue_family_index();
    init_info.Queue = *dm.graphics_queue();
    init_info.DescriptorPool = **imgui_descriptor_pool_;
    init_info.MinImageCount = 2;
    init_info.ImageCount = swapchain_image_count;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.RenderPass = render_pass;

    // 5. 初始化 Vulkan 后端
    ImGui_ImplVulkan_Init(&init_info);

    spdlog::info("ImGui Vulkan context initialized");
}

void ImGuiVulkanContext::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiVulkanContext::EndFrame(const vk::raii::CommandBuffer &cb) {
    ImGui::Render();
    if (ImDrawData* draw_data = ImGui::GetDrawData(); draw_data != nullptr) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, *cb);
    }
}

void ImGuiVulkanContext::Shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

}
