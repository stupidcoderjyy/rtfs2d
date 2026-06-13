//
// Created by PC on 2026/6/13.
//

#include "obstacle_geometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rtfs2d {

void ObstacleGeometry::AddObstacle(const std::vector<std::array<float, 2>>& points) {
    // 检查是否还能添加更多障碍物
    if (obstacles_.size() >= kMaxObstacles) return;
    if (points.size() <= 1) return;

    Obstacle obs = {};
    obs.poly_vert_count = 0;

    // 复制顶点，最多 kMaxPolyVertexes 个
    auto max_verts = static_cast<uint32_t>(kMaxPolyVerts);
    auto num_points = static_cast<uint32_t>(points.size());
    uint32_t copy_count = std::min(num_points, max_verts);

    for (uint32_t i = 0; i < copy_count; ++i) {
        obs.poly_verts[i].x = points[i][0];
        obs.poly_verts[i].y = points[i][1];
    }
    obs.poly_vert_count = copy_count;

    // 首尾闭合检测：如果首末点距离大于阈值，则追加首点
    if (copy_count >= 2) {
        float x0 = obs.poly_verts[0].x;
        float y0 = obs.poly_verts[0].y;
        float xn = obs.poly_verts[copy_count - 1].x;
        float yn = obs.poly_verts[copy_count - 1].y;
        float dx = x0 - xn;
        float dy = y0 - yn;
        if (float dist = std::sqrt(dx * dx + dy * dy); dist > 1e-6f && copy_count < max_verts) {
            obs.poly_verts[copy_count].x = x0;
            obs.poly_verts[copy_count].y = y0;
            obs.poly_vert_count = copy_count + 1;
        }
    }

    obstacles_.push_back(obs);
}

void ObstacleGeometry::Clear() {
    obstacles_.clear();
    ibm_markers_.clear();
}

void ObstacleGeometry::GenerateIBMMarkers(float h) {
    ibm_markers_.clear();

    // 半网格间距作为采样步长的上限
    float max_spacing = 0.5f * h;
    if (max_spacing <= 0.0f) return;

    for (const auto& obs : obstacles_) {
        if (obs.poly_vert_count < 2) continue;

        // 临时存储当前障碍物的标记点，以便去重
        std::vector<IBMMarker> temp_markers;

        // 遍历每条边
        for (uint32_t k = 0; k + 1 < obs.poly_vert_count; ++k) {
            const auto&[x0, y0] = obs.poly_verts[k];
            const auto&[x1, y1] = obs.poly_verts[k + 1];

            float dx = x1 - x0;
            float dy = y1 - y0;
            float length = std::sqrt(dx * dx + dy * dy);
            if (length < 1e-6f) continue;

            // 采样数量：保证间距 <= max_spacing，至少采样 1 个区间（即两个端点）
            int n_segments = std::max(1, static_cast<int>(length / max_spacing));
            float step = 1.0f / static_cast<float>(n_segments);

            // 对于第一条边，需要显式插入起点；后续边的起点由前一条边的终点自然衔接，为避免重复，
            // 在非首边时跳过起点（s=0）的插入。
            bool is_first_edge = k == 0;
            for (int s = 0; s <= n_segments; ++s) {
                if (!is_first_edge && s == 0) continue;  // 跳过与非首边的起点重复的点
                float t = static_cast<float>(s) * step;
                float x = x0 + t * dx;
                float y = y0 + t * dy;
                temp_markers.push_back({x, y});
                if (temp_markers.size() >= kMaxIBMMarkers) break;
            }
            if (temp_markers.size() >= kMaxIBMMarkers) break;
        }

        // 将生成的临时标记点加入总列表，并限制总数
        for (const auto& m : temp_markers) {
            if (ibm_markers_.size() >= kMaxIBMMarkers) {
                break;
            }
            ibm_markers_.push_back(m);
        }
        if (ibm_markers_.size() >= kMaxIBMMarkers) {
            break;
        }
    }
}

std::vector<uint8_t> ObstacleGeometry::SerializePolygonSSBO() const {
    std::vector<uint8_t> buffer;
    // 预分配空间，避免反复扩容
    size_t total_size = PolygonSSBOSize();
    buffer.reserve(total_size);

    auto write_uint32 = [&buffer](uint32_t value) {
        auto* bytes = reinterpret_cast<uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(value));
    };

    auto write_float = [&buffer](float value) {
        auto bytes = reinterpret_cast<uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(value));
    };

    write_uint32(static_cast<uint32_t>(obstacles_.size()));
    for (const auto& obs : obstacles_) {
        write_uint32(obs.poly_vert_count);
        for (uint32_t i = 0; i < obs.poly_vert_count; ++i) {
            write_float(obs.poly_verts[i].x);
            write_float(obs.poly_verts[i].y);
        }
    }
    return buffer;
}

std::vector<uint8_t> ObstacleGeometry::SerializeMarkerSSBO() const {
    std::vector<uint8_t> buffer;
    size_t total_size = MarkerSSBOSize();
    buffer.reserve(total_size);

    auto write_uint32 = [&buffer](uint32_t value) {
        auto* bytes = reinterpret_cast<uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(value));
    };

    auto write_float = [&buffer](float value) {
        auto* bytes = reinterpret_cast<uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(value));
    };

    write_uint32(static_cast<uint32_t>(ibm_markers_.size()));
    for (const auto&[x, y] : ibm_markers_) {
        write_float(x);
        write_float(y);
    }
    return buffer;
}

uint32_t ObstacleGeometry::PolygonSSBOSize() const {
    uint32_t size = 4;  // 障碍物数量
    for (const auto& obs : obstacles_) {
        size += 4;                     // vertex_count
        size += obs.poly_vert_count * 8; // 每个顶点两个 float
    }
    return size;
}

uint32_t ObstacleGeometry::MarkerSSBOSize() const {
    return 4 + static_cast<uint32_t>(ibm_markers_.size()) * 8;
}

float ObstacleGeometry::DeltaKernel1D(float r) {
    float a = std::fabs(r);
    if (a <= 1.0f) {
        float sqrt_arg = 1.0f + 4.0f * a - 4.0f * a * a;
        float sqrt_val = std::sqrt(std::max(0.0f, sqrt_arg));
        return (3.0f - 2.0f * a + sqrt_val) / 8.0f;
    }
    if (a <= 2.0f) {
        float sqrt_arg = -7.0f + 12.0f * a - 4.0f * a * a;
        float sqrt_val = std::sqrt(std::max(0.0f, sqrt_arg));
        return (5.0f - 2.0f * a - sqrt_val) / 8.0f;
    }
    return 0.0f;
}

}