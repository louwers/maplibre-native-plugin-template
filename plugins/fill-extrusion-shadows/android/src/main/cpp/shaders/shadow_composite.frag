#version 450

layout(set = 0, binding = 0) uniform sampler2D shadow_mask;
layout(push_constant) uniform CompositePush {
    float alpha;
} push;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
    float alpha = texture(shadow_mask, in_uv).r * push.alpha;
    out_color = vec4(0.0, 0.0, 0.0, alpha);
}
