//
// Created by PC on 2026/6/8.
//

#ifndef RTFS2D_GRID_H
#define RTFS2D_GRID_H
#include <cstdint>
#include <vector>

namespace rtfs2d {

struct GridParams {
    //网格结点数
    int nx, ny;
    //物理域尺寸
    float lx, ly;
    //五网格间距
    float dx, dy;

    GridParams(int nx, int ny, float lx, float ly);

    int Index(int i, int j) const {
        return i + j * nx;
    }

    int TotalCells() const {
        return nx * ny;
    }
};

class ScalarField {
public:
    explicit ScalarField(const GridParams &params);

    float& operator()(int i, int j) {
        return data_[params_.Index(i, j)];
    }

    const float& operator()(int i, int j) const {
        return data_[params_.Index(i, j)];
    }
private:
    std::vector<float> data_;
    GridParams params_;
};

class VectorField {
public:
    explicit VectorField(const GridParams &params);

    void Set(int i, int j, float u_val, float v_val) {
        u_(i, j) = u_val;
        v_(i, j) = v_val;
    }

    float U(int i, int j) const {
        return u_(i, j);
    }

    float V(int i, int j) const {
        return v_(i, j);
    }
private:
    ScalarField u_;
    ScalarField v_;
};

}


#endif //RTFS2D_GRID_H
