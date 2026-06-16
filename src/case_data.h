//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_CASE_DATA_H
#define RTFS2D_CASE_DATA_H

#include <nlohmann/json.hpp>

#include "vk_boundary.h"

namespace rtfs2d {

class ObstacleGeometry;
class BoundaryConditions;

class CaseData {
public:
    explicit CaseData(std::string name);
    void ParseJson(const nlohmann::json& json);
    std::string name() const { return name_; }
    float width() const { return width_; }
    float height() const { return height_; }
    int nx() const { return nx_; }
    int ny() const { return ny_; }
    float dx() const { return dx_; }
    float dy() const { return dy_; }
    ObstacleGeometry& geometry() const { return *geometry_; }
    BoundaryConditions& boundary() const { return *boundary_ctx_; }
    int total_cells() const { return total_cells_; }
private:
    std::string name_;
    //流场物理尺寸
    float width_, height_;
    //网格节点数
    int nx_, ny_;
    //网格间距
    float dx_;
    float dy_;
    // 障碍物
    std::unique_ptr<ObstacleGeometry> geometry_;
    // 边界条件
    std::unique_ptr<BoundaryConditions> boundary_ctx_;
    // 计算值
    int total_cells_;

    void ParseGridParams(const nlohmann::json& json);
    void ParseGeometry(const nlohmann::json& json) const;
    void ParseBoundarySide(const std::string& dir_name, BoundaryDirection dir, const nlohmann::json &side_array) const;
    void ParseBoundary(const nlohmann::json& json) const;
    void ParseVisual(const nlohmann::json& json);

    void LogCase() const;
};

}




#endif //RTFS2D_CASE_DATA_H
