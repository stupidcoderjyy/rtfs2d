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
constexpr int kBufV5 = 5;

constexpr int kBufBc0 = 6;  //Left
constexpr int kBufBc1 = 7;  //Right
constexpr int kBufBc2 = 8;  //Bottom
constexpr int kBufBc3 = 9;  //Top

constexpr int kBufIbmPolygon = 10;
constexpr int kBufIbmMarker = 11;
constexpr int kBufIbmForce = 12;
constexpr int kBufIbmMask = 13;

}

#endif //RTFS2D_BUFFERS_H
