#version 450

layout(location = 0) in ivec2 a_wall_vertex;
layout(location = 1) in ivec2 a_position_1;
layout(location = 2) in uvec2 a_decimals_edge_1;
layout(location = 3) in ivec2 a_position_2;
layout(location = 4) in uvec2 a_decimals_edge_2;
#if USE_BASE_ATTRIBUTE
layout(location = 5) in vec2 a_base;
#endif
#if USE_HEIGHT_ATTRIBUTE
layout(location = 6) in vec2 a_height;
#endif

layout(push_constant) uniform ShadowPush {
    mat4 matrix;
    float constant_base;
    float constant_height;
    float base_t;
    float height_t;
    float height_factor;
    float alpha;
    uint base_attribute;
    uint height_attribute;
} push;

vec2 unpack_float(uint packed) {
    uint first = packed / 256u;
    return vec2(first, packed - first * 256u);
}

void main() {
    if ((a_decimals_edge_1.x & 1u) != 0u) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
#if USE_HEIGHT_ATTRIBUTE
    float height = mix(a_height.x, a_height.y, push.height_t);
#else
    float height = push.constant_height;
#endif
#if USE_BASE_ATTRIBUTE
    float base = mix(a_base.x, a_base.y, push.base_t);
#else
    float base = push.constant_base;
#endif
    base = max(0.0, base);
    height = max(0.0, height);
    vec2 p1 = vec2(a_position_1) + unpack_float(a_decimals_edge_1.x / 2u) / 128.0;
    vec2 p2 = vec2(a_position_2) + unpack_float(a_decimals_edge_2.x / 2u) / 128.0;
    float upper = float(a_wall_vertex.y);
    vec2 direction = vec2(-0.5, -0.5) * (-push.height_factor) * 0.38;
    vec2 projected = (a_wall_vertex.x == 0 ? p1 : p2) + direction * mix(base, height, upper);
    gl_Position = push.matrix * vec4(projected, 0.0, 1.0);
    gl_Position.y *= -1.0;
}
