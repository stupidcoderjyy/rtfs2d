#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// Push constant 用于传递时间
layout(push_constant) uniform PushConstants {
    float time;
} pc;

void main() {
    // 横向波浪位移：基于 UV 的 y 坐标和时间
    float wave = sin(fragUV.y * 12.0 + pc.time * 2.5) * 0.06;
    vec2 uv = fragUV + vec2(wave, 0.0);

    // 彩色条纹生成：余弦波叠加出 RGB 三通道不同相位
    vec3 color = 0.5 + 0.5 * cos(uv.x * 20.0 + uv.y * 15.0 + vec3(0.0, 2.09439, 4.18879));

    outColor = vec4(color, 1.0);
}