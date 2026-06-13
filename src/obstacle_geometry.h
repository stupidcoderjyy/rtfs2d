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
    static constexpr uint32_t kMaxIBMMarkers = 256;

    struct PolyVert {
        float x, y;
    };

    //内部结构
    struct Obstacle {
        uint32_t poly_vert_count;
        PolyVert poly_verts[64];
    };

    //拉格朗日标记点
    struct IBMMarker {
        float x, y;
    };

    void AddObstacle(const std::vector<std::array<float,2>>& points);
    void GenerateIBMMarkers(float h);
    void Clear();
    std::vector<uint8_t> SerializePolygonSSBO() const;
    std::vector<uint8_t> SerializeMarkerSSBO() const;
    uint32_t PolygonSSBOSize() const;
    uint32_t MarkerSSBOSize() const;
    static float DeltaKernel1D(float r);

    uint32_t ObstacleCount() const { return obstacles_.size(); }
    const Obstacle& GetObstacle(uint32_t idx) const { return obstacles_[idx]; }
    uint32_t IBMMarkerCount() const { return ibm_markers_.size(); }
    const IBMMarker* IBMMarkerData() const { return ibm_markers_.data(); }
private:
    std::vector<Obstacle> obstacles_;
    std::vector<IBMMarker> ibm_markers_;
};


}


#endif //RTFS2D_OBSTACLE_GEOMETRY_H
