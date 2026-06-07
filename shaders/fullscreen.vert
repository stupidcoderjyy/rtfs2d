#version 450

// 硬编码的三个顶点，覆盖整个 NDC 空间（(-1,-1) 到 (3,-1) 和 (-1,3) 的三角形覆盖全屏）
vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),  // 左下角
        vec2( 3.0, -1.0),  // 右下角延伸
        vec2(-1.0,  3.0)   // 左上角延伸
);

layout(location = 0) out vec2 fragUV;

void main() {
    // 获取当前顶点位置（gl_VertexIndex 从 0 开始）
    vec2 pos = positions[gl_VertexIndex];

    // 将 NDC 坐标 (-1..3) 映射到 UV 空间 (0..1)，用于片段着色器
    // 公式：UV = (pos + 1) / 2，但由于 pos 可超出 [-1,1]，UV 也会超出 [0,1]
    fragUV = pos * 0.5 + 0.5;

    // 设置顶点位置
    gl_Position = vec4(pos, 0.0, 1.0);
}