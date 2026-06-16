//
// Created by PC on 2026/6/16.
//

#include "vis_config.h"

namespace rtfs2d {

VisConfig::VisConfig() {
    Reset();
}

void VisConfig::Reset() {
    vis_mode = VisMode::kField;
    vis_field = VisField::kVorticity;
    vis_gradient = VisGradient::kJet;
    field_coeff[0] = 1.0f;
    field_coeff[1] = 100.0f;
    field_coeff[2] = 0.07f;
    max_field_coeff[0] = 5.0f;
    max_field_coeff[1] = 500.0f;
    max_field_coeff[2] = 0.5f;
    dye_radius = 0.03f;
    dye_colors_[0] = 0x010101;
    dye_colors_[1] = 0x0DFFFF;
    dye_colors_[2] = 0x336699;
    time_step = 0.016f;
    paused = true;
}

}
