//
// Created by PC on 2026/6/8.
//

#include "Grid.h"

#include <cstdint>

using namespace rtfs2d;

GridParams::GridParams(int nx, int ny, float lx, float ly): nx(nx), ny(ny), lx(lx), ly(ly) {
    dx = lx / static_cast<float>(nx);
    dy = ly / static_cast<float>(ny);
}

ScalarField::ScalarField(const GridParams &params): params_(params) {
    data_.resize(params.TotalCells(), 0.0f);
}

VectorField::VectorField(const GridParams &params): u_(params), v_(params) {
}
