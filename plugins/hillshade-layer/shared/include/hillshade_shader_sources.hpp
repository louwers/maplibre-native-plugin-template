#pragma once

namespace maplibre::plugins::hillshade::shaders {

inline constexpr const char* glPrepareVertex = R"MLNSHADER(layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_texture_pos;

layout (std140) uniform HillshadePrepareDrawableUBO {
    highp mat4 u_matrix;
};

layout (std140) uniform HillshadePrepareTilePropsUBO {
    highp vec4 u_unpack;
    highp vec2 u_dimension;
    highp float u_zoom;
    highp float u_maxzoom;
};

out vec2 v_pos;

void main() {
    gl_Position = u_matrix * vec4(a_pos, 0, 1);

    highp vec2 epsilon = 1.0 / u_dimension;
    float scale = (u_dimension.x - 2.0) / u_dimension.x;
    v_pos = (a_texture_pos / 8192.0) * scale + epsilon;
}
)MLNSHADER";

inline constexpr const char* glPrepareFragment = R"MLNSHADER(#ifdef GL_ES
precision highp float;
#endif

in vec2 v_pos;
uniform sampler2D u_image;
layout (std140) uniform HillshadePrepareTilePropsUBO {
    highp vec4 u_unpack;
    highp vec2 u_dimension;
    highp float u_zoom;
    highp float u_maxzoom;
};

float getElevation(vec2 coord, float bias) {
    // Convert encoded elevation value to meters
    vec4 data = texture(u_image, coord) * 255.0;
    data.a = -1.0;
    return dot(data, u_unpack);
}

void main() {
    vec2 epsilon = 1.0 / u_dimension;
    float tileSize = u_dimension.x - 2.0;

    // queried pixels (using Sobel operator kernel):
    // +-----------+
    // |   |   |   |
    // | a | b | c |
    // |   |   |   |
    // +-----------+
    // |   |   |   |
    // | d | e | f |
    // |   |   |   |
    // +-----------+
    // |   |   |   |
    // | g | h | i |
    // |   |   |   |
    // +-----------+

    float a = getElevation(v_pos + vec2(-epsilon.x, -epsilon.y), 0.0);
    float b = getElevation(v_pos + vec2(0, -epsilon.y), 0.0);
    float c = getElevation(v_pos + vec2(epsilon.x, -epsilon.y), 0.0);
    float d = getElevation(v_pos + vec2(-epsilon.x, 0), 0.0);
  //float e = getElevation(v_pos, 0.0);
    float f = getElevation(v_pos + vec2(epsilon.x, 0), 0.0);
    float g = getElevation(v_pos + vec2(-epsilon.x, epsilon.y), 0.0);
    float h = getElevation(v_pos + vec2(0, epsilon.y), 0.0);
    float i = getElevation(v_pos + vec2(epsilon.x, epsilon.y), 0.0);

    // Convert the raw pixel-space derivative (slope) into world-space slope.
    // The conversion factor is: tileSize / (8 * meters_per_pixel).
    // meters_per_pixel is calculated as pow(2.0, 28.2562 - u_zoom).
    // The exaggeration factor is applied to scale the effect at lower zooms.
    // See nickidlugash's awesome breakdown for more info
    // https://github.com/mapbox/mapbox-gl-js/pull/5286#discussion_r148419556
    float exaggerationFactor = u_zoom < 2.0 ? 0.4 : u_zoom < 4.5 ? 0.35 : 0.3;
    float exaggeration = u_zoom < 15.0 ? (u_zoom - 15.0) * exaggerationFactor : 0.0;

    vec2 deriv = vec2(
        (c + f + f + i) - (a + d + d + g),
        (g + h + h + i) - (a + b + b + c)
    ) * tileSize / pow(2.0, exaggeration + (28.2562 - u_zoom));

    // Encode the derivative into the color channels (r and g)
    // The derivative is scaled from world-space slope to the range [0, 1] for texture storage.
    // The maximum possible world-space derivative is assumed to be 4 (hence division by 8.0).
    fragColor = clamp(vec4(
        deriv.x / 8.0 + 0.5,
        deriv.y / 8.0 + 0.5,
        1.0,
        1.0), 0.0, 1.0);
#ifdef OVERDRAW_INSPECTOR
    fragColor = vec4(1.0);
#endif
}
)MLNSHADER";

inline constexpr const char* glFinalVertex = R"MLNSHADER(layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_texture_pos;

layout (std140) uniform HillshadeDrawableUBO {
    highp mat4 u_matrix;
};

out vec2 v_pos;

void main() {
    gl_Position = u_matrix * vec4(a_pos, 0, 1);
    v_pos = a_texture_pos / 8192.0;
}
)MLNSHADER";

inline constexpr const char* glFinalFragment = R"MLNSHADER(in vec2 v_pos;
uniform sampler2D u_image;

layout(std140) uniform HillshadeTilePropsUBO {
    highp vec2 u_latrange;
    highp float u_exaggeration;
    highp int u_method;           // Hillshade method (0: STANDARD, 1: COMBINED, 2: IGOR, 3: MULTIDIRECTIONAL, 4: BASIC)
    highp int u_num_lights;       // Number of light sources (1-4)
    highp float u_pad0;
    highp float u_pad1;
    highp float u_pad2;
};
layout(std140) uniform HillshadeEvaluatedPropsUBO {
    highp vec4 u_accent;
    highp vec4 u_altitudes;       // Up to 4 altitude values (in radians)
    highp vec4 u_azimuths;        // Up to 4 azimuth values (in radians)
    highp vec4 u_shadows[4];      // Shadow colors (up to 4 lights)
    highp vec4 u_highlights[4];   // Highlight colors (up to 4 lights)
};

#define PI 3.141592653589793

#define STANDARD 0
#define COMBINED 1
#define IGOR 2
#define MULTIDIRECTIONAL 3
#define BASIC 4

float get_aspect(vec2 deriv) {
    return deriv.x != 0.0 ?
    atan(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
}

// MapLibre's legacy hillshade algorithm (Method 0)
void standard_hillshade(vec2 deriv) {
    float azimuth = u_azimuths.x + PI;
    float slope = atan(0.625 * length(deriv));
    float aspect = get_aspect(deriv);

    // Note: This implementation uses u_exaggeration as intensity, though it typically should be derived from u_altitudes.x.
    float intensity = u_exaggeration;

    // Scale the slope exponentially based on intensity
    float base = 1.875 - intensity * 1.75;
    float maxValue = 0.5 * PI;
    float scaledSlope = abs(intensity - 0.5) > 1e-6 ? ((pow(base, slope) - 1.0) / (pow(base, maxValue) - 1.0)) * maxValue : slope;

    float accent = cos(scaledSlope);
    vec4 accent_color = (1.0 - accent) * u_accent * clamp(intensity * 2.0, 0.0, 1.0);

    // Calculate shadow/highlight based on aspect
    float shade = abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
    vec4 shade_color = mix(u_shadows[0], u_highlights[0], shade) * sin(scaledSlope) * clamp(intensity * 2.0, 0.0, 1.0);

    fragColor = accent_color * (1.0 - shade_color.a) + shade_color;
}

// Basic directional hillshade (Method 4)
void basic_hillshade(vec2 deriv) {
    deriv = deriv * u_exaggeration * 2.0; // Exaggerate the slope derivative
    float azimuth = u_azimuths.x + PI;
    float cos_az = cos(azimuth);
    float sin_az = sin(azimuth);
    float cos_alt = cos(u_altitudes.x);
    float sin_alt = sin(u_altitudes.x);

    // Calculate the cosine of the angle between the light vector and the surface normal
    float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
    float shade = clamp(cang, 0.0, 1.0); // cang is the hillshade intensity [0, 1]

    // Blend shadow and highlight based on intensity
    if (shade > 0.5) {
        fragColor = u_highlights[0] * (2.0 * shade - 1.0); // Highlight strength [0, 1]
    } else {
        fragColor = u_shadows[0] * (1.0 - 2.0 * shade);    // Shadow strength [0, 1]
    }
}

// Multidirectional hillshade (Method 3)
void multidirectional_hillshade(vec2 deriv) {
    deriv = deriv * u_exaggeration * 2.0; // Exaggerate the slope derivative
    fragColor = vec4(0, 0, 0, 0);

    // Iterate through all light sources (up to u_num_lights, max 4)
    for (int i = 0; i < 4; i++) {
        if (i >= u_num_lights) break;

        // Access altitude and azimuth from vec4 UBOs
        float altitude = (i == 0) ? u_altitudes.x : (i == 1) ? u_altitudes.y : (i == 2) ? u_altitudes.z : u_altitudes.w;
        float azimuth = (i == 0) ? u_azimuths.x : (i == 1) ? u_azimuths.y : (i == 2) ? u_azimuths.z : u_azimuths.w;

        float cos_alt = cos(altitude);
        float sin_alt = sin(altitude);
        // Negate cos/sin azimuth for correct light direction in the normal calculation
        float cos_az = -cos(azimuth);
        float sin_az = -sin(azimuth);

        // Calculate the cosine of the angle between the light vector and the surface normal
        float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) /
                     sqrt(1.0 + dot(deriv, deriv));
        float shade = clamp(cang, 0.0, 1.0); // cang is the hillshade intensity [0, 1]

        // Accumulate shadow/highlight contribution from each light
        if (shade > 0.5) {
            fragColor += u_highlights[i] * (2.0 * shade - 1.0) / float(u_num_lights);
        } else {
            fragColor += u_shadows[i] * (1.0 - 2.0 * shade) / float(u_num_lights);
        }
    }
}

// Combined shadow and highlight method (Method 1)
void combined_hillshade(vec2 deriv) {
    // Only supports one light source (index 0)
    deriv = deriv * u_exaggeration * 2.0;
    float azimuth = u_azimuths.x + PI;
    float cos_az = cos(azimuth);
    float sin_az = sin(azimuth);
    float cos_alt = cos(u_altitudes.x);
    float sin_alt = sin(u_altitudes.x);

    // Calculate the angle between the light vector and the surface normal
    float cang = acos((sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) /
                      sqrt(1.0 + dot(deriv, deriv)));

    cang = clamp(cang, 0.0, PI / 2.0); // Clamp angle to 90 degrees (half hemisphere)

    // The "shade" and "highlight" components are calculated from cang (angle) and the magnitude of the slope
    float shade = cang * atan(length(deriv)) * 4.0 / PI / PI;
    float highlight = (PI / 2.0 - cang) * atan(length(deriv)) * 4.0 / PI / PI;

    fragColor = u_shadows[0] * shade + u_highlights[0] * highlight;
}

// Igor's shadow/highlight method (Method 2)
void igor_hillshade(vec2 deriv) {
    // Only supports one light source (index 0)
    deriv = deriv * u_exaggeration * 2.0;
    float aspect = get_aspect(deriv);
    float azimuth = u_azimuths.x + PI;

    // Slope strength is magnitude of slope vector, normalized to [0, 1]
    float slope_strength = atan(length(deriv)) * 2.0 / PI;

    // Aspect strength is difference between aspect and light azimuth, normalized to [0, 1]
    float aspect_strength = 1.0 - abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);

    float shadow_strength = slope_strength * aspect_strength;
    float highlight_strength = slope_strength * (1.0 - aspect_strength);

    fragColor = u_shadows[0] * shadow_strength + u_highlights[0] * highlight_strength;
}

void main() {
    vec4 pixel = texture(u_image, v_pos);

    // Scale the derivative based on the mercator distortion at this latitude (v_pos.y)
    float scaleFactor = cos(radians((u_latrange[0] - u_latrange[1]) * (1.0 - v_pos.y) + u_latrange[1]));

    // The derivative is scaled back from [0, 1] texture range to world-space slope
    // Texture range [0, 1] corresponds to slope range [-4, 4] (8.0 * 0.5 * deriv)
    vec2 deriv = ((pixel.rg * 8.0) - 4.0) / scaleFactor;

    // Dispatch to the selected hillshade method
    switch (u_method) {
        case BASIC:
            basic_hillshade(deriv);
            break;
        case COMBINED:
            combined_hillshade(deriv);
            break;
        case IGOR:
            igor_hillshade(deriv);
            break;
        case MULTIDIRECTIONAL:
            multidirectional_hillshade(deriv);
            break;
        case STANDARD:
        default:
            standard_hillshade(deriv);
            break;
    }

#ifdef OVERDRAW_INSPECTOR
    fragColor = vec4(1.0);
#endif
}
)MLNSHADER";

inline constexpr const char* vulkanPrepareVertex = R"MLNSHADER(

layout(location = 0) in ivec2 in_position;
layout(location = 1) in ivec2 in_texture_position;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform HillshadePrepareDrawableUBO {
    mat4 matrix;
} drawable;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_1_BINDING) uniform HillshadePrepareTilePropsUBO {
    vec4 unpack;
    vec2 dimension;
    float zoom;
    float maxzoom;
} tileProps;

layout(location = 0) out vec2 frag_position;

void main() {

    gl_Position = drawable.matrix * vec4(in_position, 0.0, 1.0);
    gl_Position.y *= -1.0;

    const vec2 epsilon = vec2(1.0) / tileProps.dimension;
    const float scale = (tileProps.dimension.x - 2.0) / tileProps.dimension.x;
    frag_position = in_texture_position / 8192.0 * scale + epsilon;
}
)MLNSHADER";

inline constexpr const char* vulkanPrepareFragment = R"MLNSHADER(

layout(location = 0) in vec2 frag_position;
layout(location = 0) out vec4 out_color;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_1_BINDING) uniform HillshadePrepareTilePropsUBO {
    vec4 unpack;
    vec2 dimension;
    float zoom;
    float maxzoom;
} tileProps;

layout(set = DRAWABLE_IMAGE_SET_INDEX, binding = MLN_PLUGIN_TEXTURE_0_BINDING) uniform sampler2D image_sampler;

float getElevation(vec2 coord, float bias, sampler2D image_sampler, vec4 unpack) {
    // Convert encoded elevation value to meters
    vec4 data = texture(image_sampler, coord) * 255.0;
    data.a = -1.0;
    return dot(data, unpack);
}

void main() {

#if defined(OVERDRAW_INSPECTOR)
    out_color = vec4(1.0);
    return;
#endif

    const vec2 epsilon = 1.0 / tileProps.dimension;
    const float tileSize = tileProps.dimension.x - 2.0;

    // queried pixels (using Sobel operator kernel):
    // +-----------+
    // |   |   |   |
    // | a | b | c |
    // |   |   |   |
    // +-----------+
    // |   |   |   |
    // | d | e | f |
    // |   |   |   |
    // +-----------+
    // |   |   |   |
    // | g | h | i |
    // |   |   |   |
    // +-----------+

    float a = getElevation(frag_position + vec2(-epsilon.x, -epsilon.y), 0.0, image_sampler, tileProps.unpack);
    float b = getElevation(frag_position + vec2(0, -epsilon.y), 0.0, image_sampler, tileProps.unpack);
    float c = getElevation(frag_position + vec2(epsilon.x, -epsilon.y), 0.0, image_sampler, tileProps.unpack);
    float d = getElevation(frag_position + vec2(-epsilon.x, 0), 0.0, image_sampler, tileProps.unpack);
  //float e = getElevation(frag_position, 0.0, image_sampler, tileProps.unpack);
    float f = getElevation(frag_position + vec2(epsilon.x, 0), 0.0, image_sampler, tileProps.unpack);
    float g = getElevation(frag_position + vec2(-epsilon.x, epsilon.y), 0.0, image_sampler, tileProps.unpack);
    float h = getElevation(frag_position + vec2(0, epsilon.y), 0.0, image_sampler, tileProps.unpack);
    float i = getElevation(frag_position + vec2(epsilon.x, epsilon.y), 0.0, image_sampler, tileProps.unpack);

    // Convert the raw pixel-space derivative (slope) into world-space slope.
    // The conversion factor is: tileSize / (8 * meters_per_pixel).
    // meters_per_pixel is calculated as pow(2.0, 28.2562 - u_zoom).
    // The exaggeration factor is applied to scale the effect at lower zooms.
    float exaggerationFactor = tileProps.zoom < 2.0 ? 0.4 : tileProps.zoom < 4.5 ? 0.35 : 0.3;
    float exaggeration = tileProps.zoom < 15.0 ? (tileProps.zoom - 15.0) * exaggerationFactor : 0.0;

    vec2 deriv = vec2(
        (c + f + f + i) - (a + d + d + g),
        (g + h + h + i) - (a + b + b + c)
    ) * tileSize / pow(2.0, exaggeration + (28.2562 - tileProps.zoom));

    // Encode the derivative into the color channels (r and g)
    // The derivative is scaled from world-space slope to the range [0, 1] for texture storage.
    // The maximum possible world-space derivative is assumed to be 4 (hence division by 8.0).
    out_color = clamp(vec4(
        deriv.x / 8.0 + 0.5,
        deriv.y / 8.0 + 0.5,
        1.0,
        1.0), 0.0, 1.0);
}
)MLNSHADER";

inline constexpr const char* vulkanFinalVertex = R"MLNSHADER(

layout(location = 0) in ivec2 in_position;
layout(location = 1) in ivec2 in_texture_position;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform HillshadeDrawableUBO {
    mat4 matrix;
} drawable;

layout(location = 0) out vec2 frag_position;

void main() {

    gl_Position = drawable.matrix * vec4(in_position, 0.0, 1.0);
    applySurfaceTransform();

    frag_position = vec2(in_texture_position) / 8192.0;
    frag_position.y = 1.0 - frag_position.y;
}
)MLNSHADER";

inline constexpr const char* vulkanFinalFragment = R"MLNSHADER(

#define PI 3.141592653589793
#define STANDARD 0
#define COMBINED 1
#define IGOR 2
#define MULTIDIRECTIONAL 3
#define BASIC 4

layout(location = 0) in vec2 frag_position;
layout(location = 0) out vec4 out_color;


layout(std140, set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_1_BINDING) uniform HillshadeTilePropsUBO {
    vec2 latrange;
    float exaggeration;
    int method;
    int num_lights;
    float pad0;
    float pad1;
    float pad2;
} tileProps;

layout(set = LAYER_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_2_BINDING) uniform HillshadeEvaluatedPropsUBO {
    vec4 accent;
    vec4 altitudes;
    vec4 azimuths;
    vec4 shadows[4];
    vec4 highlights[4];
} props;

layout(set = DRAWABLE_IMAGE_SET_INDEX, binding = MLN_PLUGIN_TEXTURE_0_BINDING) uniform sampler2D image_sampler;

float get_aspect(vec2 deriv) {
    return deriv.x != 0.0 ? atan(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
}

// MapLibre's legacy hillshade algorithm (Method 0: STANDARD)
void standard_hillshade(vec2 deriv) {
    float azimuth = props.azimuths.x + PI;
    float slope = atan(0.625 * length(deriv));
    float aspect = get_aspect(deriv);

    float intensity = tileProps.exaggeration;

    // Scale the slope exponentially based on intensity
    float base = 1.875 - intensity * 1.75;
    float maxValue = 0.5 * PI;
    float scaledSlope = abs(intensity - 0.5) > 1e-6 ? ((pow(base, slope) - 1.0) / (pow(base, maxValue) - 1.0)) * maxValue : slope;

    float accent = cos(scaledSlope);
    vec4 accent_color = (1.0 - accent) * props.accent * clamp(intensity * 2.0, 0.0, 1.0);

    float shade = abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
    vec4 shade_color = mix(props.shadows[0], props.highlights[0], shade) * sin(scaledSlope) * clamp(intensity * 2.0, 0.0, 1.0);

    out_color = accent_color * (1.0 - shade_color.a) + shade_color;
}

// Basic directional hillshade (Method 4: BASIC)
void basic_hillshade(vec2 deriv) {
    deriv = deriv * tileProps.exaggeration * 2.0;
    float azimuth = props.azimuths.x + PI;
    float cos_az = cos(azimuth);
    float sin_az = sin(azimuth);
    float cos_alt = cos(props.altitudes.x);
    float sin_alt = sin(props.altitudes.x);

    // Calculate the cosine of the angle between the light vector and the surface normal
    float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
    float shade = clamp(cang, 0.0, 1.0);

    // Blend shadow and highlight based on intensity
    if (shade > 0.5) {
        out_color = props.highlights[0] * (2.0 * shade - 1.0);
    } else {
        out_color = props.shadows[0] * (1.0 - 2.0 * shade);
    }
}

// Multidirectional hillshade (Method 3: MULTIDIRECTIONAL)
void multidirectional_hillshade(vec2 deriv) {
    deriv = deriv * tileProps.exaggeration * 2.0;
    vec4 total_color = vec4(0, 0, 0, 0);

    // Access altitude and azimuth from vec4 UBOs
    float altitudes[4] = float[4](props.altitudes.x, props.altitudes.y, props.altitudes.z, props.altitudes.w);
    float azimuths[4] = float[4](props.azimuths.x, props.azimuths.y, props.azimuths.z, props.azimuths.w);

    int num_lights = min(tileProps.num_lights, 4);

    // Iterate through all light sources
    for (int i = 0; i < num_lights; i++) {
        float altitude = altitudes[i];
        float azimuth = azimuths[i];

        float cos_alt = cos(altitude);
        float sin_alt = sin(altitude);
        // Negate cos/sin azimuth for correct light direction
        float cos_az = -cos(azimuth);
        float sin_az = -sin(azimuth);

        // Calculate the cosine of the angle between the light vector and the surface normal
        float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
        float shade = clamp(cang, 0.0, 1.0);

        // Accumulate shadow/highlight contribution from each light
        if (shade > 0.5) {
            total_color += props.highlights[i] * (2.0 * shade - 1.0) / float(num_lights);
        } else {
            total_color += props.shadows[i] * (1.0 - 2.0 * shade) / float(num_lights);
        }
    }

    out_color = total_color;
}

// Combined shadow and highlight method (Method 1: COMBINED)
void combined_hillshade(vec2 deriv) {
    // Only supports one light source (index 0)
    deriv = deriv * tileProps.exaggeration * 2.0;
    float azimuth = props.azimuths.x + PI;
    float cos_az = cos(azimuth);
    float sin_az = sin(azimuth);
    float cos_alt = cos(props.altitudes.x);
    float sin_alt = sin(props.altitudes.x);

    // Calculate the angle between the light vector and the surface normal
    float cang = acos((sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv)));

    cang = clamp(cang, 0.0, PI / 2.0);

    // Calculate shade and highlight components from angle and slope magnitude
    float shade = cang * atan(length(deriv)) * 4.0 / PI / PI;
    float highlight = (PI / 2.0 - cang) * atan(length(deriv)) * 4.0 / PI / PI;

    out_color = props.shadows[0] * shade + props.highlights[0] * highlight;
}

// Igor's shadow/highlight method (Method 2: IGOR)
void igor_hillshade(vec2 deriv) {
    // Only supports one light source (index 0)
    deriv = deriv * tileProps.exaggeration * 2.0;
    float aspect = get_aspect(deriv);
    float azimuth = props.azimuths.x + PI;

    // Slope strength is magnitude of slope vector, normalized to [0, 1]
    float slope_strength = atan(length(deriv)) * 2.0 / PI;

    // Aspect strength is difference between aspect and light azimuth, normalized to [0, 1]
    float aspect_strength = 1.0 - abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);

    float shadow_strength = slope_strength * aspect_strength;
    float highlight_strength = slope_strength * (1.0 - aspect_strength);

    out_color = props.shadows[0] * shadow_strength + props.highlights[0] * highlight_strength;
}

void main() {

#if defined(OVERDRAW_INSPECTOR)
    out_color = vec4(1.0);
    return;
#endif

    vec4 pixel = texture(image_sampler, frag_position);

    // Scale the derivative based on the mercator distortion at this latitude
    float scaleFactor = cos(radians((tileProps.latrange[0] - tileProps.latrange[1]) * frag_position.y + tileProps.latrange[1]));

    // The derivative is scaled back from [0, 1] texture range to world-space slope
    // Texture range [0, 1] corresponds to slope range [-4, 4]
    vec2 deriv = ((pixel.rg * 8.0) - 4.0) / scaleFactor;

    // Dispatch to the selected hillshade method
    if (tileProps.method == BASIC) {
        basic_hillshade(deriv);
    } else if (tileProps.method == COMBINED) {
        combined_hillshade(deriv);
    } else if (tileProps.method == IGOR) {
        igor_hillshade(deriv);
    } else if (tileProps.method == MULTIDIRECTIONAL) {
        multidirectional_hillshade(deriv);
    } else {
        // Default to STANDARD
        standard_hillshade(deriv);
    }
}
)MLNSHADER";

inline constexpr const char* metalPrepare = R"MLNSHADER(



struct alignas(16) HillshadePrepareDrawableUBO {
    /*  0 */ float4x4 matrix;
    /* 64 */
};
static_assert(sizeof(HillshadePrepareDrawableUBO) == 4 * 16, "wrong size");

struct alignas(16) HillshadePrepareTilePropsUBO {
    /*  0 */ float4 unpack;
    /* 16 */ float2 dimension;
    /* 24 */ float zoom;
    /* 28 */ float maxzoom;
    /* 32 */
};
static_assert(sizeof(HillshadePrepareTilePropsUBO) == 2 * 16, "wrong size");



struct VertexStage {
    short2 pos [[attribute(0)]];
    short2 texture_pos [[attribute(1)]];
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float2 pos;
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const HillshadePrepareDrawableUBO& drawable [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]],
                                device const HillshadePrepareTilePropsUBO& tileProps [[buffer(MLN_PLUGIN_UNIFORM_1_BINDING)]]) {

    const float4 position = drawable.matrix * float4(float2(vertx.pos), 0, 1);

    float2 epsilon = 1.0 / tileProps.dimension;
    float scale = (tileProps.dimension.x - 2.0) / tileProps.dimension.x;
    float2 pos = (float2(vertx.texture_pos) / 8192.0) * scale + epsilon;

    return {
        .position    = position,
        .pos         = pos,
    };
}

float getElevation(float2 coord, float bias, texture2d<float, access::sample> image, sampler image_sampler, float4 unpack) {
    // Convert encoded elevation value to meters
    float4 data = image.sample(image_sampler, coord) * 255.0;
    data.a = -1.0;
    return dot(data, unpack);
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]],
                            device const HillshadePrepareTilePropsUBO& tileProps [[buffer(MLN_PLUGIN_UNIFORM_1_BINDING)]],
                            texture2d<float, access::sample> image [[texture(MLN_PLUGIN_TEXTURE_0_BINDING)]],
                            sampler image_sampler [[sampler(MLN_PLUGIN_TEXTURE_0_BINDING)]]) {
#if defined(OVERDRAW_INSPECTOR)
    return half4(1.0);
#endif

    float2 epsilon = 1.0 / tileProps.dimension;
    float tileSize = tileProps.dimension.x - 2.0;

    // queried pixels (using Sobel operator kernel):
    // +-----------+
    // |   |   |   |
    // | a | b | c |
    // |   |   |   |
    // +-----------+
    // |   |   |   |
    // | d | e | f |
    // |   |   |   |
    // +-----------+
    // |   |   |   |
    // | g | h | i |
    // |   |   |   |
    // +-----------+
    float a = getElevation(in.pos + float2(-epsilon.x, -epsilon.y), 0.0, image, image_sampler, tileProps.unpack);
    float b = getElevation(in.pos + float2(0, -epsilon.y), 0.0, image, image_sampler, tileProps.unpack);
    float c = getElevation(in.pos + float2(epsilon.x, -epsilon.y), 0.0, image, image_sampler, tileProps.unpack);
    float d = getElevation(in.pos + float2(-epsilon.x, 0), 0.0, image, image_sampler, tileProps.unpack);
  //float e = getElevation(in.pos, 0.0, image, image_sampler, tileProps.unpack);
    float f = getElevation(in.pos + float2(epsilon.x, 0), 0.0, image, image_sampler, tileProps.unpack);
    float g = getElevation(in.pos + float2(-epsilon.x, epsilon.y), 0.0, image, image_sampler, tileProps.unpack);
    float h = getElevation(in.pos + float2(0, epsilon.y), 0.0, image, image_sampler, tileProps.unpack);
    float i = getElevation(in.pos + float2(epsilon.x, epsilon.y), 0.0, image, image_sampler, tileProps.unpack);

    // Convert the raw pixel-space derivative (slope) into world-space slope.
    // The conversion factor is: tileSize / (8 * meters_per_pixel).
    // meters_per_pixel is calculated as pow(2.0, 28.2562 - u_zoom).
    // The exaggeration factor is applied to scale the effect at lower zooms.
    float exaggerationFactor = tileProps.zoom < 2.0 ? 0.4 : tileProps.zoom < 4.5 ? 0.35 : 0.3;
    float exaggeration = tileProps.zoom < 15.0 ? (tileProps.zoom - 15.0) * exaggerationFactor : 0.0;

    float2 deriv = float2(
        (c + f + f + i) - (a + d + d + g),
        (g + h + h + i) - (a + b + b + c)
    ) * tileSize / pow(2.0, exaggeration + (28.2562 - tileProps.zoom));

    // Encode the derivative into the color channels (r and g)
    // The derivative is scaled from world-space slope to the range [0, 1] for texture storage.
    // The maximum possible world-space derivative is assumed to be 4 (hence division by 8.0).
    float4 color = clamp(float4(
        deriv.x / 8.0 + 0.5,
        deriv.y / 8.0 + 0.5,
        1.0,
        1.0), 0.0, 1.0);

    return half4(color);
}
)MLNSHADER";

inline constexpr const char* metalFinal = R"MLNSHADER(



struct alignas(16) HillshadeDrawableUBO {
    /*  0 */ float4x4 matrix;
    /* 64 */
};
static_assert(sizeof(HillshadeDrawableUBO) == 4 * 16, "wrong size");

struct alignas(16) HillshadeTilePropsUBO {
    /*  0 */ float2 latrange;
    /*  8 */ float exaggeration;
    /* 12 */ int32_t method;
    /* 16 */ int32_t num_lights;
    /* 20 */ float pad0;
    /* 24 */ float pad1;
    /* 28 */ float pad2;
    /* 32 */
};
static_assert(sizeof(HillshadeTilePropsUBO) == 2 * 16, "wrong size");

/// Evaluated properties that do not depend on the tile
struct alignas(16) HillshadeEvaluatedPropsUBO {
    /*  0 */ float4 accent;
    /* 16 */ float4 altitudes;       // Up to 4 altitude values (in radians)
    /* 32 */ float4 azimuths;        // Up to 4 azimuth values (in radians)
    /* 48 */ float4 shadows[4];      // Shadow colors (up to 4 lights)
    /* 112 */ float4 highlights[4];  // Highlight colors (up to 4 lights)
    /* 176 */
};
static_assert(sizeof(HillshadeEvaluatedPropsUBO) == 11 * 16, "wrong size");



#define PI 3.141592653589793
#define STANDARD 0
#define COMBINED 1
#define IGOR 2
#define MULTIDIRECTIONAL 3
#define BASIC 4

struct VertexStage {
    short2 pos [[attribute(0)]];
    short2 texture_pos [[attribute(1)]];
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float2 pos;
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const HillshadeDrawableUBO& drawable [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {

    const float4 position = drawable.matrix * float4(float2(vertx.pos), 0, 1);
    float2 pos = float2(vertx.texture_pos) / 8192.0;
    // Metal's texture coordinate origin differs from some renderers;
    // restore Y-flip to match prepare pass and historical behavior.
    pos.y = 1.0 - pos.y;

    return {
        .position    = position,
        .pos         = pos,
    };
}

// Helper function to calculate aspect (normalized direction of slope)
float get_aspect(float2 deriv) {
    return deriv.x != 0.0 ? atan2(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
}

// MapLibre's legacy hillshade algorithm (Method 0: STANDARD)
void standard_hillshade(float2 deriv, device const HillshadeTilePropsUBO& tileProps, device const HillshadeEvaluatedPropsUBO& props, thread half4& fragColor) {
    float azimuth = props.azimuths.x + PI;
    float slope = atan(0.625 * length(deriv));
    float aspect = get_aspect(deriv);

    float intensity = tileProps.exaggeration;

    // Scale the slope exponentially based on intensity
    float base = 1.875 - intensity * 1.75;
    float maxValue = 0.5 * PI;
    float scaledSlope = abs(intensity - 0.5) > 1e-6 ? ((pow(base, slope) - 1.0) / (pow(base, maxValue) - 1.0)) * maxValue : slope;

    float accent = cos(scaledSlope);
    float4 accent_color = (1.0 - accent) * props.accent * clamp(intensity * 2.0, 0.0, 1.0);

    float shade = abs(glMod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
    float4 shade_color = mix(props.shadows[0], props.highlights[0], shade) * sin(scaledSlope) * clamp(intensity * 2.0, 0.0, 1.0);

    fragColor = half4(accent_color * (1.0 - shade_color.a) + shade_color);
}

// Basic directional hillshade (Method 4: BASIC)
void basic_hillshade(float2 deriv, device const HillshadeTilePropsUBO& tileProps, device const HillshadeEvaluatedPropsUBO& props, thread half4& fragColor) {
    deriv = deriv * tileProps.exaggeration * 2.0;
    float azimuth = props.azimuths.x + PI;
    float cos_az = cos(azimuth);
    float sin_az = sin(azimuth);
    float cos_alt = cos(props.altitudes.x);
    float sin_alt = sin(props.altitudes.x);

    // Calculate the cosine of the angle between the light vector and the surface normal
    float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
    float shade = clamp(cang, 0.0, 1.0);

    // Blend shadow and highlight based on intensity
    if (shade > 0.5) {
        fragColor = half4(props.highlights[0] * (2.0 * shade - 1.0));
    } else {
        fragColor = half4(props.shadows[0] * (1.0 - 2.0 * shade));
    }
}

// Multidirectional hillshade (Method 3: MULTIDIRECTIONAL)
void multidirectional_hillshade(float2 deriv, device const HillshadeTilePropsUBO& tileProps, device const HillshadeEvaluatedPropsUBO& props, thread half4& fragColor) {
    deriv = deriv * tileProps.exaggeration * 2.0;
    float4 total_color = float4(0, 0, 0, 0);

    int num_lights = min(tileProps.num_lights, 4);

    // Iterate through all light sources
    for (int i = 0; i < num_lights; i++) {
        // Access altitude and azimuth from vec4 UBOs
        float altitude = (i == 0) ? props.altitudes.x : (i == 1) ? props.altitudes.y : (i == 2) ? props.altitudes.z : props.altitudes.w;
        float azimuth = (i == 0) ? props.azimuths.x : (i == 1) ? props.azimuths.y : (i == 2) ? props.azimuths.z : props.azimuths.w;

        float cos_alt = cos(altitude);
        float sin_alt = sin(altitude);
        // Negate cos/sin azimuth for correct light direction
        float cos_az = -cos(azimuth);
        float sin_az = -sin(azimuth);

        // Calculate the cosine of the angle between the light vector and the surface normal
        float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
        float shade = clamp(cang, 0.0, 1.0);

        // Accumulate shadow/highlight contribution from each light
        if (shade > 0.5) {
            total_color += props.highlights[i] * (2.0 * shade - 1.0) / float(num_lights);
        } else {
            total_color += props.shadows[i] * (1.0 - 2.0 * shade) / float(num_lights);
        }
    }

    fragColor = half4(total_color);
}

// Combined shadow and highlight method (Method 1: COMBINED)
void combined_hillshade(float2 deriv, device const HillshadeTilePropsUBO& tileProps, device const HillshadeEvaluatedPropsUBO& props, thread half4& fragColor) {
    // Only supports one light source (index 0)
    deriv = deriv * tileProps.exaggeration * 2.0;
    float azimuth = props.azimuths.x + PI;
    float cos_az = cos(azimuth);
    float sin_az = sin(azimuth);
    float cos_alt = cos(props.altitudes.x);
    float sin_alt = sin(props.altitudes.x);

    // Calculate the angle between the light vector and the surface normal
    float cang = acos((sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv)));

    cang = clamp(cang, 0.0, PI / 2.0);

    // Calculate shade and highlight components from angle and slope magnitude
    float shade = cang * atan(length(deriv)) * 4.0 / PI / PI;
    float highlight = (PI / 2.0 - cang) * atan(length(deriv)) * 4.0 / PI / PI;

    fragColor = half4(props.shadows[0] * shade + props.highlights[0] * highlight);
}

// Igor's shadow/highlight method (Method 2: IGOR)
void igor_hillshade(float2 deriv, device const HillshadeTilePropsUBO& tileProps, device const HillshadeEvaluatedPropsUBO& props, thread half4& fragColor) {
    // Only supports one light source (index 0)
    deriv = deriv * tileProps.exaggeration * 2.0;
    float aspect = get_aspect(deriv);
    float azimuth = props.azimuths.x + PI;

    // Slope strength is magnitude of slope vector, normalized to [0, 1]
    float slope_strength = atan(length(deriv)) * 2.0 / PI;

    // Aspect strength is difference between aspect and light azimuth, normalized to [0, 1]
    float aspect_strength = 1.0 - abs(glMod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);

    float shadow_strength = slope_strength * aspect_strength;
    float highlight_strength = slope_strength * (1.0 - aspect_strength);

    fragColor = half4(props.shadows[0] * shadow_strength + props.highlights[0] * highlight_strength);
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]],
                            device const HillshadeTilePropsUBO& tileProps [[buffer(MLN_PLUGIN_UNIFORM_1_BINDING)]],
                            device const HillshadeEvaluatedPropsUBO& props [[buffer(MLN_PLUGIN_UNIFORM_2_BINDING)]],
                            texture2d<float, access::sample> image [[texture(MLN_PLUGIN_TEXTURE_0_BINDING)]],
                            sampler image_sampler [[sampler(MLN_PLUGIN_TEXTURE_0_BINDING)]]) {
#if defined(OVERDRAW_INSPECTOR)
    return half4(1.0);
#endif
    thread half4 fragColor;

    float4 pixel = image.sample(image_sampler, in.pos);

    // Scale the derivative based on the mercator distortion at this latitude
    float latitude = (tileProps.latrange.x - tileProps.latrange.y) * in.pos.y + tileProps.latrange.y;
    float scaleFactor = cos(radians(latitude));

    // The derivative is scaled back from [0, 1] texture range to world-space slope
    // Texture range [0, 1] corresponds to slope range [-4, 4]
    float2 deriv = ((pixel.rg * 8.0) - 4.0) / scaleFactor;

    // Dispatch to the selected hillshade method
    if (tileProps.method == BASIC) {
        basic_hillshade(deriv, tileProps, props, fragColor);
    } else if (tileProps.method == COMBINED) {
        combined_hillshade(deriv, tileProps, props, fragColor);
    } else if (tileProps.method == IGOR) {
        igor_hillshade(deriv, tileProps, props, fragColor);
    } else if (tileProps.method == MULTIDIRECTIONAL) {
        multidirectional_hillshade(deriv, tileProps, props, fragColor);
    } else {
        // Default to STANDARD
        standard_hillshade(deriv, tileProps, props, fragColor);
    }

    return fragColor;
}
)MLNSHADER";

} // namespace maplibre::plugins::hillshade::shaders
