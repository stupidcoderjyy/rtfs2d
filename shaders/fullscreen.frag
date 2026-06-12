#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(constant_id = 0) const uint NX = 128u;
layout(constant_id = 1) const uint NY = 128u;

// 绑定与 ComputeContext::RecordFluidStepCommands 结束状态保持一致
layout(set = 0, binding = 0, std430) buffer VelocityUBuffer {
    float vel_u[];
};
layout(set = 0, binding = 1, std430) buffer VelocityVBuffer {
    float vel_v[];
};
layout(set = 0, binding = 2, std430) buffer PressureBuffer {
    float p[];
};

void main() {
    uint i = uint(fragUV.x * float(NX));
    uint j = uint(fragUV.y * float(NY));
    i = clamp(i, 0u, NX - 1u);
    j = clamp(j, 0u, NY - 1u);
    uint idx = i + j * NX;

    // 读取速度分量
    float u = vel_u[idx];
    float v = vel_v[idx];

    // 计算速度幅值（标量强度）
    float mag = length(vec2(u, v));;

    // 直接使用幅值作为灰度亮度（clamp 到 [0,1]，超出部分截断）
    float intensity = clamp(mag, 0.0, 1.0);
    outColor = vec4(intensity, intensity, intensity,1.0);
}