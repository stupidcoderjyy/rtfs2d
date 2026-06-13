//
// Created by PC on 2026/6/13.
//

#ifndef RTFS2D_BUFFERS_H
#define RTFS2D_BUFFERS_H

namespace rtfs2d::buffers {

constexpr int kBufV0 = 0;
constexpr int kBufV1 = 1;
constexpr int kBufV2 = 2;
constexpr int kBufV3 = 3;
constexpr int kBufV4 = 4;

constexpr int kBufBc0 = 5;  //Left
constexpr int kBufBc1 = 6;  //Right
constexpr int kBufBc2 = 7;  //Bottom
constexpr int kBufBc3 = 8;  //Top

constexpr int kBufIbmPolygon = 9;
constexpr int kBufIbmMarker = 10;
constexpr int kBufIbmForce = 11;
constexpr int kBufIbmMask = 12;

}

#endif //RTFS2D_BUFFERS_H
