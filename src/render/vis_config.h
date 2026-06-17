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
    VisMode vis_mode{};
    VisGradient vis_gradient{};
    VisField vis_field{};
    std::array<float, 3> field_coeff{};
    std::array<float, 3> max_field_coeff{};
    float dye_radius{};
    std::array<uint32_t, 3> dye_colors_{};
    float time_step{};
    float viscosity{};
    bool paused{};

    VisConfig();
    void Reset();

    float& CurrentFieldCoeff() {
        return field_coeff[static_cast<int32_t>(vis_field)];
    }

    float CurrentMaxFieldCoeff() const {
        return max_field_coeff[static_cast<int32_t>(vis_field)];
    }
};

}
#endif //RTFS2D_VIS_CONFIG_H
