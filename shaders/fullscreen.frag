#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(constant_id = 0) const uint NX = 128u;
layout(constant_id = 1) const uint NY = 128u;

layout(set = 0, binding = 0, std430) buffer VelocityUBuffer { float vel_u[]; };
layout(set = 0, binding = 1, std430) buffer VelocityVBuffer { float vel_v[]; };
layout(set = 0, binding = 2, std430) buffer PressureBuffer { float p[]; };
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
        outColor = vec4(0.05, 0.05, 0.08, 1.0);
        return;
    }

    if (edge < 0.003) {
        outColor = vec4(0.3, 0.6, 1.0, 1.0);
        return;
    }

    uint i = mi;
    uint j = mj;
    uint idx = i + j * NX;

    float u = vel_u[idx];
    float v = vel_v[idx];
    float mag = length(vec2(u, v));
    float intensity = clamp(mag, 0.0, 1.0);
    outColor = vec4(intensity, intensity, intensity, 1.0);
}