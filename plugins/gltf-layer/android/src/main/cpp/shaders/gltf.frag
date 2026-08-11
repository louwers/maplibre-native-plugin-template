#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec4 v_color;

layout(push_constant) uniform PushConstants {
    mat4 matrix;
    float opacity;
} u;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 light = normalize(vec3(-0.45, -0.55, 0.75));
    float diffuse = 0.35 + 0.65 * abs(dot(normalize(v_normal), light));
    float alpha = clamp(v_color.a * u.opacity, 0.0, 1.0);
    out_color = vec4(v_color.rgb * diffuse * alpha, alpha);
}
