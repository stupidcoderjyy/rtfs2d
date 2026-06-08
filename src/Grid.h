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
    
}


#endif //RTFS2D_GRID_H
