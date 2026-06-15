//
// Created by PC on 2026/6/8.
//

#ifndef RTFS2D_GRID_H
#define RTFS2D_GRID_H

namespace rtfs2d {

struct GridParams {
    //网格节点数
    int nx, ny;
    //网格间距（单位域[0,1]x[0,1]）
    float dx = 1.0f / static_cast<float>(nx);
    float dy = 1.0f / static_cast<float>(ny);

    GridParams(int nx, int ny);
    int Index(int i, int j) const { return i + j * nx; }
    int TotalCells() const { return nx * ny; }
};

}


#endif //RTFS2D_GRID_H
