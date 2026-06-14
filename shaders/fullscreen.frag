#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(constant_id = 0) const uint NX = 128u;
layout(constant_id = 1) const uint NY = 128u;

layout(set = 0, binding = 2, std430) buffer ScalarBuffer { float s[]; };
layout(set = 0, binding = 12, std430) buffer MaskBuf { float data[]; } mask;

void main() {
    uint mi = uint(fragUV.x * float(NX));
    uint mj = uint(fragUV.y * float(NY));
    mi = clamp(mi, 0u, NX - 1u);
    mj = clamp(mj, 0u, NY - 1u);
    uint midx = mi + mj * NX;

    float inside = mask.data[midx * 2u];
    float edge  = mask.data[midx * 2u + 1u];

    if (inside > 0.5) {
        outColor = vec4(0, 0, 0, 1.0);
        return;
    }

    if (edge < 0.002) {
        outColor = vec4(0.3, 0.6, 1.0, 1.0);
        return;
    }

    float scalar = clamp(s[midx], 0.0, 1.0);
    outColor = vec4(scalar, scalar, scalar, 1.0);
}