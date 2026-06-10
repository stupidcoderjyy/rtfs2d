#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// 绑定与 ComputeContext::RecordFluidStepCommands 结束状态保持一致
layout(set = 0, binding = 2, std430) buffer VelocityUBuffer {
    float vel_u[];
};
layout(set = 0, binding = 3, std430) buffer VelocityVBuffer {
    float vel_v[];
};

void main() {
    const uint nx = 128u;
    const uint ny = 128u;

    uint i = uint(fragUV.x * float(nx));
    uint j = uint(fragUV.y * float(ny));
    i = clamp(i, 0u, nx - 1u);
    j = clamp(j, 0u, ny - 1u);
    uint idx = i + j * nx;

    // 读取速度分量
    float u = vel_u[idx];
    float v = vel_v[idx];

    // 计算速度幅值（标量强度）
    float mag = length(vec2(u, v));

    // 直接使用幅值作为灰度亮度（clamp 到 [0,1]，超出部分截断）
    float intensity = clamp(mag, 0.0, 1.0);
    outColor = vec4(intensity, intensity, intensity, 1.0);
}