//
// Created by PC on 2026/6/16.
//

#ifndef RTFS2D_VIS_CONFIG_H
#define RTFS2D_VIS_CONFIG_H

#include <array>
#include <cstdint>

namespace rtfs2d {

enum class VisField { kSpeed = 0, kPressure = 1, kVorticity = 2 };
enum class VisGradient { kGray = 0, kJet = 1, kCoolWarm = 2 };
enum class VisMode { kField = 0, kDye = 1 };


struct VisConfig {
    VisMode vis_mode = VisMode::kField;
    VisGradient vis_gradient = VisGradient::kJet;
    VisField vis_field = VisField::kVorticity;
    std::array<float, 3> field_coeff{ 1.0f, 100.0f, 0.07f };
    std::array<float, 3> max_field_coeff{ 5.0f, 500.0f, 0.5f};
    float dye_radius = 0.03f;
    std::array<uint32_t, 3> dye_colors_{ 0x010101, 0x0DFFFF, 0x336699};
    float time_step = 0.016f;

    float& CurrentFieldCoeff() {
        return field_coeff[static_cast<int32_t>(vis_field)];
    }

    float CurrentMaxFieldCoeff() const {
        return max_field_coeff[static_cast<int32_t>(vis_field)];
    }
};

}
#endif //RTFS2D_VIS_CONFIG_H
