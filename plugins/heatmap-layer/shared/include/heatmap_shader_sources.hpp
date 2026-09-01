#pragma once

namespace maplibre::plugins::heatmap::shaders {

inline constexpr const char* glKernelVertex = R"MLNSHADER(
layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_corner;
#if !MLN_PLUGIN_PROPERTY_HEATMAP_WEIGHT_IS_UNIFORM
layout (location = 2) in float a_weight_min;
layout (location = 3) in float a_weight_max;
#endif
#if !MLN_PLUGIN_PROPERTY_HEATMAP_RADIUS_IS_UNIFORM
layout (location = 4) in float a_radius_min;
layout (location = 5) in float a_radius_max;
#endif

layout (std140) uniform HeatmapKernelUBO {
    highp mat4 u_matrix;
    highp float u_pixels_to_tile_units;
    highp float u_intensity;
    highp float u_pad0;
    highp float u_pad1;
    highp float u_weight;
    highp float u_radius;
    highp float u_property_pad0;
    highp float u_property_pad1;
    highp vec4 u_interpolation;
};

out highp float v_weight;
out mediump vec2 v_extrude;

const highp float ZERO = 1.0 / 255.0 / 16.0;
#define GAUSS_COEF 0.3989422804014327

void main() {
    float weight = u_weight;
    float radius = u_radius;
#if !MLN_PLUGIN_PROPERTY_HEATMAP_WEIGHT_IS_UNIFORM
    weight = mix(a_weight_min, a_weight_max, u_interpolation.x);
#endif
#if !MLN_PLUGIN_PROPERTY_HEATMAP_RADIUS_IS_UNIFORM
    radius = mix(a_radius_min, a_radius_max, u_interpolation.y);
#endif
    weight = max(weight, 0.0);
    radius = max(radius, 1.0);
    float S = sqrt(-2.0 * log(ZERO / (max(weight, ZERO) * max(u_intensity, ZERO) * GAUSS_COEF))) / 3.0;
    v_extrude = S * a_corner;
    vec2 extrude = v_extrude * radius * u_pixels_to_tile_units;
    gl_Position = u_matrix * vec4(a_position + extrude, 0.0, 1.0);
    v_weight = weight;
}
)MLNSHADER";

inline constexpr const char* glKernelFragment = R"MLNSHADER(
#ifdef GL_ES
precision highp float;
#endif

layout (std140) uniform HeatmapKernelUBO {
    highp mat4 u_matrix;
    highp float u_pixels_to_tile_units;
    highp float u_intensity;
    highp float u_pad0;
    highp float u_pad1;
    highp float u_weight;
    highp float u_radius;
    highp float u_property_pad0;
    highp float u_property_pad1;
    highp vec4 u_interpolation;
};

in highp float v_weight;
in mediump vec2 v_extrude;

#define GAUSS_COEF 0.3989422804014327

void main() {
    float d = -0.5 * 3.0 * 3.0 * dot(v_extrude, v_extrude);
    float value = v_weight * u_intensity * GAUSS_COEF * exp(d);
    fragColor = vec4(value, 1.0, 1.0, 1.0);
#ifdef OVERDRAW_INSPECTOR
    fragColor = vec4(1.0);
#endif
}
)MLNSHADER";

inline constexpr const char* glCompositeVertex = R"MLNSHADER(
layout (location = 0) in vec2 a_position;

layout (std140) uniform HeatmapCompositeUBO {
    highp mat4 u_matrix;
    highp float u_opacity;
    highp float u_pad0;
    highp float u_pad1;
    highp float u_pad2;
};

out vec2 v_position;

void main() {
    gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);
    v_position = vec2(a_position.x, 1.0 - a_position.y);
}
)MLNSHADER";

inline constexpr const char* glCompositeFragment = R"MLNSHADER(
#ifdef GL_ES
precision highp float;
#endif

in vec2 v_position;
uniform sampler2D u_density;
uniform sampler2D u_color_ramp;

layout (std140) uniform HeatmapCompositeUBO {
    highp mat4 u_matrix;
    highp float u_opacity;
    highp float u_pad0;
    highp float u_pad1;
    highp float u_pad2;
};

void main() {
    float density = texture(u_density, v_position).r;
    fragColor = texture(u_color_ramp, vec2(density, 0.5)) * u_opacity;
#ifdef OVERDRAW_INSPECTOR
    fragColor = vec4(0.0);
#endif
}
)MLNSHADER";

inline constexpr const char* vulkanKernelVertex = R"MLNSHADER(
layout(location = 0) in ivec2 in_position;
layout(location = 1) in ivec2 in_corner;
#if !MLN_PLUGIN_PROPERTY_HEATMAP_WEIGHT_IS_UNIFORM
layout(location = 2) in float in_weight_min;
layout(location = 3) in float in_weight_max;
#endif
#if !MLN_PLUGIN_PROPERTY_HEATMAP_RADIUS_IS_UNIFORM
layout(location = 4) in float in_radius_min;
layout(location = 5) in float in_radius_max;
#endif

layout(std140, set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform HeatmapKernelUBO {
    mat4 matrix;
    float pixels_to_tile_units;
    float intensity;
    float pad0;
    float pad1;
    float weight;
    float radius;
    float property_pad0;
    float property_pad1;
    vec4 interpolation;
} props;

layout(location = 0) out float frag_weight;
layout(location = 1) out vec2 frag_extrude;

const float ZERO = 1.0 / 255.0 / 16.0;
#define GAUSS_COEF 0.3989422804014327

void main() {
    float weight = props.weight;
    float radius = props.radius;
#if !MLN_PLUGIN_PROPERTY_HEATMAP_WEIGHT_IS_UNIFORM
    weight = mix(in_weight_min, in_weight_max, props.interpolation.x);
#endif
#if !MLN_PLUGIN_PROPERTY_HEATMAP_RADIUS_IS_UNIFORM
    radius = mix(in_radius_min, in_radius_max, props.interpolation.y);
#endif
    weight = max(weight, 0.0);
    radius = max(radius, 1.0);
    float S = sqrt(-2.0 * log(ZERO / (max(weight, ZERO) * max(props.intensity, ZERO) * GAUSS_COEF))) / 3.0;
    frag_extrude = S * vec2(in_corner);
    vec2 extrude = frag_extrude * radius * props.pixels_to_tile_units;
    gl_Position = props.matrix * vec4(vec2(in_position) + extrude, 0.0, 1.0);
    applySurfaceTransform();
    frag_weight = weight;
}
)MLNSHADER";

inline constexpr const char* vulkanKernelFragment = R"MLNSHADER(
layout(location = 0) in float frag_weight;
layout(location = 1) in vec2 frag_extrude;
layout(location = 0) out vec4 out_color;

layout(std140, set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform HeatmapKernelUBO {
    mat4 matrix;
    float pixels_to_tile_units;
    float intensity;
    float pad0;
    float pad1;
    float weight;
    float radius;
    float property_pad0;
    float property_pad1;
    vec4 interpolation;
} props;

#define GAUSS_COEF 0.3989422804014327

void main() {
#if defined(OVERDRAW_INSPECTOR)
    out_color = vec4(1.0);
    return;
#endif
    float d = -0.5 * 3.0 * 3.0 * dot(frag_extrude, frag_extrude);
    float value = frag_weight * props.intensity * GAUSS_COEF * exp(d);
    out_color = vec4(value, 1.0, 1.0, 1.0);
}
)MLNSHADER";

inline constexpr const char* vulkanCompositeVertex = R"MLNSHADER(
layout(location = 0) in ivec2 in_position;

layout(set = LAYER_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform HeatmapCompositeUBO {
    mat4 matrix;
    float opacity;
    float pad0;
    float pad1;
    float pad2;
} props;

layout(location = 0) out vec2 frag_position;

void main() {
    gl_Position = props.matrix * vec4(in_position, 0.0, 1.0);
    applySurfaceTransform();
    frag_position = vec2(in_position);
}
)MLNSHADER";

inline constexpr const char* vulkanCompositeFragment = R"MLNSHADER(
layout(location = 0) in vec2 frag_position;
layout(location = 0) out vec4 out_color;

layout(set = LAYER_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform HeatmapCompositeUBO {
    mat4 matrix;
    float opacity;
    float pad0;
    float pad1;
    float pad2;
} props;

layout(set = DRAWABLE_IMAGE_SET_INDEX, binding = MLN_PLUGIN_TEXTURE_0_BINDING) uniform sampler2D density_sampler;
layout(set = DRAWABLE_IMAGE_SET_INDEX, binding = MLN_PLUGIN_TEXTURE_1_BINDING) uniform sampler2D color_ramp_sampler;

void main() {
#if defined(OVERDRAW_INSPECTOR)
    out_color = vec4(0.0);
    return;
#endif
    float density = texture(density_sampler, frag_position).r;
    out_color = texture(color_ramp_sampler, vec2(density, 0.5)) * props.opacity;
}
)MLNSHADER";

inline constexpr const char* metalKernel = R"MLNSHADER(
struct alignas(16) HeatmapKernelUBO {
    float4x4 matrix;
    float pixels_to_tile_units;
    float intensity;
    float pad0;
    float pad1;
    float weight;
    float radius;
    float property_pad0;
    float property_pad1;
    float4 interpolation;
};

struct VertexStage {
    short2 position [[attribute(0)]];
    short2 corner [[attribute(1)]];
#if !MLN_PLUGIN_PROPERTY_HEATMAP_WEIGHT_IS_UNIFORM
    float weight_min [[attribute(2)]];
    float weight_max [[attribute(3)]];
#endif
#if !MLN_PLUGIN_PROPERTY_HEATMAP_RADIUS_IS_UNIFORM
    float radius_min [[attribute(4)]];
    float radius_max [[attribute(5)]];
#endif
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float weight;
    float2 extrude;
};

constant const float ZERO = 1.0 / 255.0 / 16.0;
#define GAUSS_COEF 0.3989422804014327

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const HeatmapKernelUBO& props [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {
    float weight = props.weight;
    float radius = props.radius;
#if !MLN_PLUGIN_PROPERTY_HEATMAP_WEIGHT_IS_UNIFORM
    weight = mix(vertx.weight_min, vertx.weight_max, props.interpolation.x);
#endif
#if !MLN_PLUGIN_PROPERTY_HEATMAP_RADIUS_IS_UNIFORM
    radius = mix(vertx.radius_min, vertx.radius_max, props.interpolation.y);
#endif
    weight = max(weight, 0.0);
    radius = max(radius, 1.0);
    float S = sqrt(-2.0 * log(ZERO / (max(weight, ZERO) * max(props.intensity, ZERO) * GAUSS_COEF))) / 3.0;
    float2 extrude = S * float2(vertx.corner);
    float2 offset = extrude * radius * props.pixels_to_tile_units;
    return {
        .position = props.matrix * float4(float2(vertx.position) + offset, 0.0, 1.0),
        .weight = weight,
        .extrude = extrude,
    };
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]],
                            device const HeatmapKernelUBO& props [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {
#if defined(OVERDRAW_INSPECTOR)
    return half4(1.0);
#endif
    float d = -0.5 * 3.0 * 3.0 * dot(in.extrude, in.extrude);
    float value = in.weight * props.intensity * GAUSS_COEF * exp(d);
    return half4(value, 1.0, 1.0, 1.0);
}
)MLNSHADER";

inline constexpr const char* metalComposite = R"MLNSHADER(
struct alignas(16) HeatmapCompositeUBO {
    float4x4 matrix;
    float opacity;
    float pad0;
    float pad1;
    float pad2;
};

struct VertexStage {
    short2 position [[attribute(0)]];
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float2 texture_position;
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const HeatmapCompositeUBO& props [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {
    float2 position = float2(vertx.position);
    return {
        .position = props.matrix * float4(position, 0.0, 1.0),
        .texture_position = position,
    };
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]],
                            device const HeatmapCompositeUBO& props [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]],
                            texture2d<float, access::sample> density [[texture(MLN_PLUGIN_TEXTURE_0_BINDING)]],
                            texture2d<float, access::sample> color_ramp [[texture(MLN_PLUGIN_TEXTURE_1_BINDING)]],
                            sampler density_sampler [[sampler(MLN_PLUGIN_TEXTURE_0_BINDING)]],
                            sampler color_ramp_sampler [[sampler(MLN_PLUGIN_TEXTURE_1_BINDING)]]) {
#if defined(OVERDRAW_INSPECTOR)
    return half4(0.0);
#endif
    float value = density.sample(density_sampler, in.texture_position).r;
    return half4(color_ramp.sample(color_ramp_sampler, float2(value, 0.5)) * props.opacity);
}
)MLNSHADER";

} // namespace maplibre::plugins::heatmap::shaders
