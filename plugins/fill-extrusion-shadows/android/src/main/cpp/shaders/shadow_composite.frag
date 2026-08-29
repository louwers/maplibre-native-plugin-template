#version 450

layout(set = 0, binding = 0) uniform sampler2D shadow_mask;
layout(push_constant) uniform CompositePush {
    float alpha;
    float rotation_cos;
    float rotation_sin;
} push;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 position = in_uv * 2.0 - 1.0;
    vec2 rotated = vec2(push.rotation_cos * position.x + push.rotation_sin * position.y,
                        -push.rotation_sin * position.x + push.rotation_cos * position.y);
    vec2 mask_uv = rotated * 0.5 + 0.5;
    float alpha = texture(shadow_mask, mask_uv).r * push.alpha;
    out_color = vec4(0.0, 0.0, 0.0, alpha);
}
