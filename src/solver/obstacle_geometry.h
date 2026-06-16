//
// Created by PC on 2026/6/13.
//

#ifndef RTFS2D_OBSTACLE_GEOMETRY_H
#define RTFS2D_OBSTACLE_GEOMETRY_H
#include <cstdint>
#include <vector>
#include <array>

namespace rtfs2d {

class ObstacleGeometry {
public:
    static constexpr uint32_t kMaxObstacles = 16;
    static constexpr uint32_t kMaxPolyVerts = 64;
    static constexpr uint32_t kMaxMarkers = 2048;

    struct PolyVert {
        float x, y;
    };

    struct Obstacle {
        uint32_t vert_count{};
        alignas(8) std::array<float, 4> aabb;   // std430 规定vec2为8字节对齐
        std::array<PolyVert, kMaxPolyVerts> poly_verts;
        void ComputeAABB();
    };

    struct PolygonSSBO {
        uint32_t count{};
        alignas(8) std::array<Obstacle, kMaxObstacles> obstacles;
    };

    //拉格朗日标记点
    struct Marker {
        float x, y;
        //占位，着色器中赋值
        std::array<float, 4> _padding;
    };

    struct MarkerSSBO {
        uint32_t count{};
        std::array<Marker, kMaxMarkers> markers;
    };

    static constexpr uint32_t kMarkBufferSize = sizeof(MarkerSSBO);
    static constexpr uint32_t kPolygonBufferSize = sizeof(PolygonSSBO);

    void AddObstacle(const std::vector<std::array<float,2>>& points);
    void GenerateIBMMarkers(float h);
    void Clear();
    const PolygonSSBO& polygon_ssbo() const { return polygon_ssbo_; }
    std::vector<uint8_t> SerializePolygonSSBO() const;
    std::vector<uint8_t> SerializeMarkerSSBO() const;
    uint32_t MarksCount() const {
        return marker_ssbo_.count;
    }
private:
    PolygonSSBO polygon_ssbo_{};
    MarkerSSBO marker_ssbo_{};
};


}


#endif //RTFS2D_OBSTACLE_GEOMETRY_H
