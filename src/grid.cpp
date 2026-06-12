//
// Created by PC on 2026/6/8.
//

#include "grid.h"

#include <cstdint>

using namespace rtfs2d;

GridParams::GridParams(int nx, int ny): nx(nx), ny(ny) {
}

ScalarField::ScalarField(const GridParams &params): params_(params) {
    data_.resize(params.TotalCells(), 0.0f);
}

VectorField::VectorField(const GridParams &params): u_(params), v_(params) {
}
