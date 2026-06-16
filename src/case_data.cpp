// Created by PC on 2026/6/15.

#include "case_data.h"

#include <spdlog/spdlog.h>

#include "obstacle_geometry.h"
#include "boundary_conditions.h"
#include "compile/compiler_input.h"
#include "obstacle_exp_parser.h"

namespace rtfs2d {

boundary_conditions::boundary_conditions(std::string name): name_(std::move(name)), width_(), height_(), nx_(), ny_(), dx_(), dy_(), total_cells_() {
    boundary_ctx_ = std::make_unique<BoundaryConditions>();
    geometry_ = std::make_unique<ObstacleGeometry>();
}

void boundary_conditions::ParseJson(const nlohmann::json& json) {
    if (!json.is_object()) {
        throw std::runtime_error("JSON root must be an object");
    }
    auto it_case = json.find("case");
    if (it_case == json.end() || !it_case->is_object()) {
        throw std::runtime_error("Missing or invalid 'case' field");
    }
    const auto& case_obj = *it_case;
    ParseGridParams(case_obj);
    ParseGeometry(case_obj);
    ParseBoundary(case_obj);
    LogCase();
}

void boundary_conditions::ParseGridParams(const nlohmann::json& json) {
    if (auto it = json.find("mesh_resolution"); it != json.end() && it->is_array() && it->size() == 2) {
        nx_ = (*it)[0].get<int>();
        ny_ = (*it)[1].get<int>();
    } else {
        throw std::runtime_error("Missing or invalid 'mesh_resolution' (must be [nx, ny])");
    }

    if (auto it = json.find("mesh_density"); it != json.end() && it->is_number()) {
        float density = it->get<float>();
        width_ = density * static_cast<float>(nx_);
        height_ = density * static_cast<float>(ny_);
        dx_ = density;
        dy_ = density;
    } else {
        throw std::runtime_error("Missing or invalid 'mesh_density' (must be a number)");
    }

    total_cells_ = nx_ * ny_;
}

void boundary_conditions::ParseGeometry(const nlohmann::json& json) const {
    auto it = json.find("geometry");
    if (it == json.end() || !it->is_array()) {
        return;
    }

    for (const auto& item : *it) {
        if (!item.is_string()) {
            throw std::runtime_error("Each geometry item must be a string");
        }
        auto desc = item.get<std::string>();
        auto input = CompilerInput::FromString(desc);
        ObstacleExpParser parser(*geometry_);
        try {
            ObstacleExpLexer lexer;
            parser.Run(lexer, *input);
        } catch (CompileError& err) {
            spdlog::error(err.FormatErrorMessage());
        }
    }
}

static BoundaryType ParseType(const std::string& s) {
    if (s == "no_slip_wall") return BoundaryType::kNoSlipWall;
    if (s == "slip_wall")    return BoundaryType::kSlipWall;
    if (s == "velocity")     return BoundaryType::kVelocity;
    if (s == "pressure")     return BoundaryType::kPressure;
    throw std::runtime_error("Unknown boundary type: " + s);
}

void boundary_conditions::ParseBoundarySide(const std::string& dir_name, BoundaryDirection dir, const nlohmann::json& side_array) const {
    if (!side_array.is_array()) {
        throw std::runtime_error("Boundary condition must be an array");
    }
    for (const auto& cond : side_array) {
        if (!cond.is_object()) {
            throw std::runtime_error("Each boundary condition must be an object");
        }
        float from, to;
        if (cond.contains("from") && cond["from"].is_number()) {
            from = cond["from"].get<double>();
        } else {
            throw std::runtime_error("Missing or invalid 'from' in boundary condition");
        }
        if (cond.contains("to") && cond["to"].is_number()) {
            to = cond["to"].get<double>();
        } else {
            throw std::runtime_error("Missing or invalid 'to' in boundary condition");
        }
        if (from > to) {
            throw std::runtime_error("'from' must be <= 'to' in boundary condition");
        }

        if (!cond.contains("type") || !cond["type"].is_string()) {
            throw std::runtime_error("Missing or invalid 'type' in boundary condition");
        }
        auto bc_type = ParseType(cond["type"].get<std::string>());

        float u = 0.0f, v = 0.0f;
        if (cond.contains("u") && cond["u"].is_number()) {
            u = cond["u"].get<float>();
        }
        if (cond.contains("v") && cond["v"].is_number()) {
            v = cond["v"].get<float>();
        }
        boundary_ctx_->SetBoundary(dir, bc_type, from, to, u, v);
    }
}

void boundary_conditions::ParseBoundary(const nlohmann::json& json) const {
    auto it = json.find("boundary_conditions");
    if (it == json.end() || !it->is_object()) {
        return;
    }
    const auto& bc_obj = *it;

    static constexpr std::pair<const char*, BoundaryDirection> kSideMap[] = {
        {"left", BoundaryDirection::kLeft},
        {"right", BoundaryDirection::kRight},
        {"bottom", BoundaryDirection::kBottom},
        {"top", BoundaryDirection::kTop},
    };

    for (const auto& [name, dir] : kSideMap) {
        if (auto it_side = bc_obj.find(name); it_side != bc_obj.end()) {
            ParseBoundarySide(name, dir, *it_side);
        }
    }
}

void boundary_conditions::LogCase() const {
    std::ostringstream oss;
    oss << "Case Loaded\n";
    oss << "Field info:\n";
    oss << "    " << std::left << std::setw(20) << "Field Width:" << width_ << '\n';
    oss << "    " << std::left << std::setw(20) << "Field Height:" << height_ << '\n';
    oss << "    " << std::left << std::setw(20) << "Mesh Resolution:" << nx_ << " x " << ny_ << '\n';

    oss << "Geometry info:\n";
    oss << "    " << std::left;
    oss << std::setw(20) << "Obstacle" << "Vertexes\n";
    const auto&[count, obstacles] = geometry_->polygon_ssbo();
    for (int i = 0; i < count; ++i) {
        const auto& obs = obstacles[i];
        oss << "    ";
        oss << std::setw(20) << std::format("[{}]", i);
        oss << std::setw(20) << obs.vert_count << '\n';
    }

    oss << std::left << "Boundary Conditions:\n";
    oss << "    " << std::setw(15) << "Direction";
    oss << std::setw(10) << "From";
    oss << std::setw(10) << "To";
    oss << std::setw(15) << "Type";
    oss << std::setw(10) << "U";
    oss << std::setw(10) << "V" << '\n';
    const auto& bc = boundary_ctx_->segments();
    for (int i = 0; i < 4; ++i) {
        for (const auto&[begin, end, type, u, v] : bc[i]) {
            constexpr std::array<const char*, 4> kTypeNames{ "NoSlipWall", "SlipWall", "Velocity", "Pressure" };
            constexpr std::array<const char*, 4> kDirectionNames{ "Left", "Right", "Bottom", "Top" };
            oss << "    ";
            oss << std::setw(15) << kDirectionNames[i];
            oss << std::setw(10) << begin;
            oss << std::setw(10) << end;
            oss << std::setw(15) << kTypeNames[static_cast<int>(type)];
            if (type == BoundaryType::kVelocity) {
                oss << std::setw(10) << u;
                oss << v;
            }
            oss << '\n';
        }
    }

    auto s = oss.str();
    s.pop_back();
    spdlog::info(s);
}

}
