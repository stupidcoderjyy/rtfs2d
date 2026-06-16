//
// Created by PC on 2026/6/13.
//

#include "obstacle_geometry.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <spdlog/spdlog.h>

#include "vk_memory.h"

namespace rtfs2d {

void ObstacleGeometry::Obstacle::ComputeAABB() {
        if (vert_count == 0) return;
        float min_x = poly_verts[0].x, min_y = poly_verts[0].y;
        float max_x = min_x, max_y = min_y;
        for (uint32_t i = 1; i < vert_count; ++i) {
            float x = poly_verts[i].x, y = poly_verts[i].y;
            if (x < min_x) min_x = x; if (x > max_x) max_x = x;
            if (y < min_y) min_y = y; if (y > max_y) max_y = y;
        }
        // 在 AABB 边界上略微外扩以避免射线投射精度问题
        constexpr float kEps = 1e-5f;
        aabb[0] = min_x - kEps;
        aabb[1] = min_y - kEps;
        aabb[2] = max_x + kEps;
        aabb[3] = max_y + kEps;
}

void ObstacleGeometry::AddObstacle(const std::vector<std::array<float, 2>>& points) {
    // 检查是否还能添加更多障碍物
    if (polygon_ssbo_.count >= kMaxObstacles) {
        spdlog::warn("Obstacle ignored: Max obstacles reached({})", kMaxObstacles);
        return;
    }
    if (points.size() <= 1) {
        spdlog::warn("Obstacle ignored: No polygon points");
        return;
    }
    auto& obs = polygon_ssbo_.obstacles[polygon_ssbo_.count++];
    obs.vert_count = 0;

    // 复制顶点，最多 kMaxPolyVertexes 个
    auto max_verts = static_cast<uint32_t>(kMaxPolyVerts);
    auto num_points = static_cast<uint32_t>(points.size());
    if (num_points > max_verts) {
        spdlog::warn("Max vertices reached({}), {} vertex(es) ignored.", kMaxPolyVerts, num_points - max_verts);
    }
    uint32_t copy_count = std::min(num_points, max_verts);
    for (uint32_t i = 0; i < copy_count; ++i) {
        obs.poly_verts[i].x = points[i][0];
        obs.poly_verts[i].y = points[i][1];
    }
    obs.vert_count = copy_count;
    obs.ComputeAABB();
}

void ObstacleGeometry::Clear() {
    polygon_ssbo_.count = 0;
    marker_ssbo_.count = 0;
}

void ObstacleGeometry::GenerateIBMMarkers(float h) {
    marker_ssbo_.count = 0;

    // 半网格间距作为采样步长的上限
    float max_spacing = 0.5f * h;
    if (max_spacing <= 0.0f) {
        throw std::runtime_error("Negative or zero grid height:" + std::to_string(h));
    }

    int marks_capacity = kMaxMarkers;

    for (int i = 0; i < polygon_ssbo_.count; ++i) {
        const auto& [vtc, aabb, vts] = polygon_ssbo_.obstacles[i];
        if (vtc < 2) {
            continue;
        }
        // 遍历每条边
        for (uint32_t k = 0; k + 1 < vtc; ++k) {
            const auto&[x0, y0] = vts[k];
            const auto&[x1, y1] = vts[k + 1];

            float dx = x1 - x0;
            float dy = y1 - y0;
            float length = std::sqrt(dx * dx + dy * dy);
            if (length < 1e-6f) {
                spdlog::warn("Ignore coincident points: ({},{}) ({},{})", x0, y0, x1, y1);
                continue;
            }

            // 采样数量：保证间距 <= max_spacing，至少采样 1 个区间（即两个端点）
            int n_segments = std::max(1, static_cast<int>(length / max_spacing));
            if (n_segments > marks_capacity) {
                spdlog::warn("Early termination of sampling on edge {} of obstacle"
                    " {}: markers reached kMaxIBMMarkers({})", k, i, kMaxMarkers);
            }
            float step = 1.0f / static_cast<float>(n_segments);

            // 对于第一条边，需要显式插入起点；后续边的起点由前一条边的终点自然衔接，为避免重复，
            // 在非首边时跳过起点（s=0）的插入。
            bool is_first_edge = k == 0;
            for (int s = 0; s <= n_segments; ++s) {
                if (!is_first_edge && s == 0) {
                    continue;  // 跳过与非首边的起点重复的点
                }
                if (marks_capacity-- == 0) {
                    return;  //放弃采样
                }
                float t = static_cast<float>(s) * step;
                auto& [x, y, d] = marker_ssbo_.markers[marker_ssbo_.count++];
                x = x0 + t * dx;
                y = y0 + t * dy;
            }
        }
    }
}

std::vector<uint8_t> ObstacleGeometry::SerializePolygonSSBO() const {
    std::vector<uint8_t> buffer;
    AppendDataToBytesVec(buffer, polygon_ssbo_);
    return buffer;
}

std::vector<uint8_t> ObstacleGeometry::SerializeMarkerSSBO() const {
    std::vector<uint8_t> buffer;
    AppendDataToBytesVec(buffer, marker_ssbo_);
    return buffer;
}

}