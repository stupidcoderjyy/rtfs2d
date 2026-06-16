#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(constant_id = 0) const uint NX = 128u;
layout(constant_id = 1) const uint NY = 128u;
layout(push_constant) uniform VisParams {
    uint gradientType;
    uint visMode;
    uint dyeColorLow;
    uint dyeColorMid;
    uint dyeColorHigh;
} vis;

layout(set = 0, binding = 2, std430) buffer ScalarBuffer { float s[]; };
layout(set = 0, binding = 12, std430) buffer MaskBuf { float data[]; } mask;
layout(set = 0, binding = 13, std430) buffer DyeBuffer { float d[]; };

vec3 GrayScale(float t) {
    t = clamp(t, 0.0, 1.0);
    return vec3(t);
}

vec3 Jet(float t) {
    t = clamp(t, 0.0, 1.0);
    if (t < 0.125) {
        return mix(vec3(0.0, 0.0, 0.5), vec3(0.0, 0.0, 1.0), t / 0.125);
    } else if (t < 0.375) {
        return mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), (t - 0.125) / 0.25);
    } else if (t < 0.625) {
        return mix(vec3(0.0, 1.0, 1.0), vec3(1.0, 1.0, 0.0), (t - 0.375) / 0.25);
    } else if (t < 0.875) {
        return mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.625) / 0.25);
    } else {
        return mix(vec3(1.0, 0.0, 0.0), vec3(0.5, 0.0, 0.0), (t - 0.875) / 0.125);
    }
}

vec3 CoolWarm(float t) {
    t = clamp(t, 0.0, 1.0);
    float blue = pow(1.0 - t, 3.0);
    float red  = pow(t, 3.0);
    float white = 6.0 * t * (1.0 - t);
    return vec3(red + white, white, blue + white);
}

vec3 unpackColor(uint c) {
    return vec3(float((c >> 16) & 0xFFu) / 255.0,
                float((c >> 8)  & 0xFFu) / 255.0,
                float( c        & 0xFFu) / 255.0);
}

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

    if (edge < 0.3) {
        outColor = vec4(0.3, 0.6, 1.0, 1.0);
        return;
    }

    if (vis.visMode == 0u) {
        float scalar = clamp(s[midx], 0.0, 1.0);
        vec3 color;
        if (vis.gradientType == 0u) {
            color = GrayScale(scalar);
        } else if (vis.gradientType == 1u) {
            color = Jet(scalar);
        } else {
            color = CoolWarm(scalar);
        }
        outColor = vec4(color, 1.0);
    } else {
        vec3 color_low  = unpackColor(vis.dyeColorLow);
        vec3 color_mid  = unpackColor(vis.dyeColorMid);
        vec3 color_high = unpackColor(vis.dyeColorHigh);
        float dye_val = clamp(d[midx], 0.0, 1.0);
        vec3 dye_color = mix(color_low, mix(color_mid, color_high, dye_val), dye_val);
        outColor = vec4(dye_color, 1.0);
    }
}
