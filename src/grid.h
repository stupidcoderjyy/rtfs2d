//
// Created by PC on 2026/6/8.
//

#ifndef RTFS2D_GRID_H
#define RTFS2D_GRID_H

namespace rtfs2d {

struct GridParams {
    //流场物理尺寸
    float width, height;
    //网格节点数
    int nx, ny;
    //网格间距（单位域[0,1]x[0,1]）
    float dx;
    float dy;

    GridParams(float width, float height, int nx, int ny)
        : width(width), height(height), nx(nx), ny(ny) {
        dx = width / nx;
        dy = height / ny;
    }
    int Index(int i, int j) const { return i + j * nx; }
    int TotalCells() const { return nx * ny; }
};

}


#endif //RTFS2D_GRID_H
