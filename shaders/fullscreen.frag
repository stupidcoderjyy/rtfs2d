#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// 存储缓冲区：绑定到 set = 0, binding = 0，包含场数据
layout(set = 0, binding = 0, std430) buffer FieldBuffer {
    float values[];
};

void main() {
    // 网格分辨率（必须与 CPU 端的 grid_params_ 一致）
    uint nx = 128u;
    uint ny = 128u;

    // 根据纹理坐标计算网格索引
    uint i = uint(fragUV.x * float(nx));
    uint j = uint(fragUV.y * float(ny));

    // 防止越界
    i = clamp(i, 0u, nx - 1u);
    j = clamp(j, 0u, ny - 1u);

    // 线性索引（行优先）
    uint idx = i + j * nx;

    // 读取场值
    float val = values[idx];

    // 输出灰度颜色
    outColor = vec4(vec3(val), 1.0);
}