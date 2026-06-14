//
// Created by PC on 2026/6/13.
//

#ifndef RTFS2D_BUFFERS_H
#define RTFS2D_BUFFERS_H

namespace rtfs2d::buffers {

constexpr int kBufV0 = 0x00;
constexpr int kBufV1 = 0x01;
constexpr int kBufV2 = 0x02;
constexpr int kBufV3 = 0x03;
constexpr int kBufV4 = 0x04;
constexpr int kBufV5 = 0x05;
constexpr int kBufV6 = 0x06;

constexpr int kBufBc0 = 0x10;  //Left
constexpr int kBufBc1 = 0x11;  //Right
constexpr int kBufBc2 = 0x12;  //Bottom
constexpr int kBufBc3 = 0x13;  //Top

constexpr int kBufIbmPolygon = 0x20;
constexpr int kBufIbmMarker = 0x21;
constexpr int kBufIbmForce = 0x22;
constexpr int kBufIbmMask = 0x23;

}

#endif //RTFS2D_BUFFERS_H
