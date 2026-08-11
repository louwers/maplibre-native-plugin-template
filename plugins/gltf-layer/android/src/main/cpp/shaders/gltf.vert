#version 450

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;

layout(push_constant) uniform PushConstants {
    mat4 matrix;
    float opacity;
} u;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec4 v_color;

void main() {
    gl_Position = u.matrix * vec4(a_position, 1.0);
    v_normal = normalize(vec3(a_normal.x, -a_normal.z, a_normal.y));
    v_color = a_color;
}
