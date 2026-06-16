//
// Created by PC on 2026/6/16.
//

#include "imgui_control_panel.h"

#include <imgui.h>
#include "vis_config.h"

namespace rtfs2d {

ImGuiControlPanel::ImGuiControlPanel(VisConfig& config) : config_(&config) {}

void ImGuiControlPanel::Render() const {
    ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // ---- Pause / Reset (公共) ----
    if (ImGui::Button(config_->paused ? "Resume" : "Pause")) {
        config_->paused = !config_->paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        config_->Reset();
    }

    // ---- 时间步长 (两种模式共享) ----
    ImGui::SliderFloat("Time Step", &config_->time_step, 0.001f, 0.05f, "%.4f");

    // ---- VisMode 下拉框 ----
    static auto vis_mode_names = "Field\0Dye\0\0";
    int vis_mode_idx = static_cast<int>(config_->vis_mode);
    if (ImGui::Combo("Mode", &vis_mode_idx, vis_mode_names)) {
        config_->vis_mode = static_cast<VisMode>(vis_mode_idx);
    }

    ImGui::Separator();

    // ---- 条件渲染 ----
    if (config_->vis_mode == VisMode::kField) {
        RenderFieldControls();
    } else {
        RenderDyeControls();
    }

    ImGui::End();
}

void ImGuiControlPanel::RenderFieldControls() const {
    // VisGradient 下拉框
    static const char* grad_names = "Grayscale\0Jet\0Cool/Warm\0\0";
    int grad_idx = static_cast<int>(config_->vis_gradient);
    if (ImGui::Combo("Gradient", &grad_idx, grad_names)) {
        config_->vis_gradient = static_cast<VisGradient>(grad_idx);
    }

    // VisField 下拉框
    static auto field_names = "Speed\0Pressure\0Vorticity\0\0";
    int field_idx = static_cast<int>(config_->vis_field);
    if (ImGui::Combo("Field", &field_idx, field_names)) {
        config_->vis_field = static_cast<VisField>(field_idx);
    }

    // 系数滑条
    ImGui::SliderFloat("Coefficient",
        &config_->CurrentFieldCoeff(),
        0.0f,
        config_->CurrentMaxFieldCoeff(), "%.4f");
}

void ImGuiControlPanel::RenderDyeControls() const {
    // 墨迹半径滑条
    ImGui::SliderFloat("Ink Radius", &config_->dye_radius, 0.005f, 0.1f, "%.3f");

    auto unpack = [](uint32_t c, float col[3]) {
        col[0] = static_cast<float>((c >> 16) & 0xFFu) / 255.0f;
        col[1] = static_cast<float>((c >> 8)  & 0xFFu) / 255.0f;
        col[2] = static_cast<float>( c        & 0xFFu) / 255.0f;
    };
    auto pack = [](const float col[3]) -> uint32_t {
        uint32_t r = static_cast<uint32_t>(col[0] * 255.0f) & 0xFFu;
        uint32_t g = static_cast<uint32_t>(col[1] * 255.0f) & 0xFFu;
        uint32_t b = static_cast<uint32_t>(col[2] * 255.0f) & 0xFFu;
        return r << 16 | g << 8 | b;
    };

    float col[3];

    unpack(config_->dye_colors_[0], col);
    if (ImGui::ColorEdit3("Ink Low", col)) {
        config_->dye_colors_[0] = pack(col);
    }

    unpack(config_->dye_colors_[1], col);
    if (ImGui::ColorEdit3("Ink Mid", col)) {
        config_->dye_colors_[1] = pack(col);
    }

    unpack(config_->dye_colors_[2], col);
    if (ImGui::ColorEdit3("Ink High", col)) {
        config_->dye_colors_[2] = pack(col);
    }
}

}
