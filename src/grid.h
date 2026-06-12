//
// Created by PC on 2026/6/8.
//

#ifndef RTFS2D_GRID_H
#define RTFS2D_GRID_H
#include <vector>

namespace rtfs2d {

struct GridParams {
    //网格节点数
    int nx, ny;
    //网格间距（单位域[0,1]x[0,1]）
    float dx = 1.0f / static_cast<float>(nx);
    float dy = 1.0f / static_cast<float>(ny);

    GridParams(int nx, int ny);

    int Index(int i, int j) const {
        return i + j * nx;
    }

    int TotalCells() const {
        return nx * ny;
    }

    int InteriorCells() const {
        return (nx - 2) * (ny - 2);
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
