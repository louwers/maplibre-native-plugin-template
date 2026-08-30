#include "hillshade_layer.hpp"
#include "hillshade_shader_sources.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#ifndef MLN_HILLSHADE_PLUGIN_VERSION
#define MLN_HILLSHADE_PLUGIN_VERSION "0.1.0-local"
#endif

namespace {

using namespace maplibre::plugins::hillshade;

constexpr mln_plugin_string str(const char* value, size_t size) { return {value, size}; }

template <size_t N>
constexpr mln_plugin_string str(const char (&value)[N]) {
    return str(value, N - 1);
}

constexpr mln_plugin_value makeFloat(float value) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_FLOAT;
    result.data.float_value = value;
    return result;
}

constexpr mln_plugin_value makeString(const char* value, size_t size) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_STRING;
    result.data.string_value = str(value, size);
    return result;
}

template <size_t N>
constexpr mln_plugin_value makeString(const char (&value)[N]) {
    return makeString(value, N - 1);
}

constexpr mln_plugin_value makeColor(float r, float g, float b, float a) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_COLOR;
    result.data.color_value = {r, g, b, a};
    return result;
}

constexpr mln_plugin_value makeFloatArray(const float* values, size_t count) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_FLOAT_ARRAY;
    result.data.float_array_value = {values, count};
    return result;
}

constexpr mln_plugin_value makeColorArray(const mln_plugin_color* values, size_t count) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_COLOR_ARRAY;
    result.data.color_array_value = {values, count};
    return result;
}

mln_plugin_property_descriptor_v1 propertyDescriptor(mln_plugin_string name,
                                                     mln_plugin_value_type type,
                                                     mln_plugin_value defaultValue,
                                                     bool acceptsScalar = false,
                                                     std::optional<float> minimum = {},
                                                     std::optional<float> maximum = {},
                                                     uint32_t maximumArrayLength = 0,
                                                     const mln_plugin_string* enumValues = nullptr,
                                                     size_t enumValueCount = 0) {
    mln_plugin_property_descriptor_v1 property{};
    property.struct_size = sizeof(property);
    property.name = name;
    property.type = type;
    property.scope = MLN_PLUGIN_PROPERTY_PAINT;
    property.default_value = defaultValue;
    property.supports_expressions = 1;
    property.supports_transitions = 0;
    property.accepts_scalar = acceptsScalar;
    property.has_minimum = minimum.has_value();
    property.has_maximum = maximum.has_value();
    property.minimum = minimum.value_or(0.0f);
    property.maximum = maximum.value_or(0.0f);
    property.maximum_array_length = maximumArrayLength;
    property.enum_values = enumValues;
    property.enum_value_count = enumValueCount;
    return property;
}

constexpr std::array<float, 1> defaultAltitude = {45.0f};
constexpr std::array<float, 1> defaultDirection = {335.0f};
constexpr std::array<mln_plugin_color, 1> defaultHighlight = {{{1.0f, 1.0f, 1.0f, 1.0f}}};
constexpr std::array<mln_plugin_color, 1> defaultShadow = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
constexpr std::array<mln_plugin_string, 2> anchorValues = {str("map"), str("viewport")};
constexpr std::array<mln_plugin_string, 5> methodValues = {
    str("standard"), str("basic"), str("combined"), str("igor"), str("multidirectional")};

const std::array<mln_plugin_property_descriptor_v1, 8> properties = {{
    propertyDescriptor(str("hillshade-accent-color"),
                       MLN_PLUGIN_VALUE_COLOR,
                       makeColor(0.0f, 0.0f, 0.0f, 1.0f)),
    propertyDescriptor(str("hillshade-exaggeration"),
                       MLN_PLUGIN_VALUE_FLOAT,
                       makeFloat(0.5f),
                       false,
                       0.0f,
                       1.0f),
    propertyDescriptor(str("hillshade-highlight-color"),
                       MLN_PLUGIN_VALUE_COLOR_ARRAY,
                       makeColorArray(defaultHighlight.data(), defaultHighlight.size()),
                       true,
                       {},
                       {},
                       4),
    propertyDescriptor(str("hillshade-illumination-altitude"),
                       MLN_PLUGIN_VALUE_FLOAT_ARRAY,
                       makeFloatArray(defaultAltitude.data(), defaultAltitude.size()),
                       true,
                       0.0f,
                       90.0f,
                       4),
    propertyDescriptor(str("hillshade-illumination-anchor"),
                       MLN_PLUGIN_VALUE_STRING,
                       makeString("viewport"),
                       false,
                       {},
                       {},
                       0,
                       anchorValues.data(),
                       anchorValues.size()),
    propertyDescriptor(str("hillshade-illumination-direction"),
                       MLN_PLUGIN_VALUE_FLOAT_ARRAY,
                       makeFloatArray(defaultDirection.data(), defaultDirection.size()),
                       true,
                       0.0f,
                       359.0f,
                       4),
    propertyDescriptor(str("hillshade-method"),
                       MLN_PLUGIN_VALUE_STRING,
                       makeString("standard"),
                       false,
                       {},
                       {},
                       0,
                       methodValues.data(),
                       methodValues.size()),
    propertyDescriptor(str("hillshade-shadow-color"),
                       MLN_PLUGIN_VALUE_COLOR_ARRAY,
                       makeColorArray(defaultShadow.data(), defaultShadow.size()),
                       true,
                       {},
                       {},
                       4),
}};

const mln_plugin_value* property(const mln_plugin_uniform_context_v1& context, const char* name) {
    const auto length = std::strlen(name);
    for (size_t i = 0; i < context.property_count; ++i) {
        const auto& candidate = context.properties[i];
        if (candidate.name.data && candidate.name.size == length &&
            std::memcmp(candidate.name.data, name, length) == 0) {
            return &candidate.value;
        }
    }
    return nullptr;
}

float number(const mln_plugin_uniform_context_v1& context, const char* name, float fallback) {
    const auto* value = property(context, name);
    return value && value->type == MLN_PLUGIN_VALUE_FLOAT ? value->data.float_value : fallback;
}

std::string_view stringValue(const mln_plugin_uniform_context_v1& context,
                             const char* name,
                             std::string_view fallback) {
    const auto* value = property(context, name);
    if (!value || value->type != MLN_PLUGIN_VALUE_STRING || !value->data.string_value.data) return fallback;
    return {value->data.string_value.data, value->data.string_value.size};
}

struct alignas(16) PrepareDrawableUBO {
    float matrix[16];
};

struct alignas(16) PrepareTileUBO {
    float unpack[4];
    float dimension[2];
    float zoom;
    float maxzoom;
};

struct alignas(16) FinalDrawableUBO {
    float matrix[16];
};

struct alignas(16) FinalTileUBO {
    float latitudeRange[2];
    float exaggeration;
    int32_t method;
    int32_t numberOfLights;
    float padding[3];
};

struct alignas(16) EvaluatedUBO {
    std::array<float, 4> accent;
    std::array<float, 4> altitudes;
    std::array<float, 4> azimuths;
    std::array<float, 16> shadows;
    std::array<float, 16> highlights;
};

static_assert(sizeof(PrepareDrawableUBO) == 64);
static_assert(sizeof(PrepareTileUBO) == 32);
static_assert(sizeof(FinalDrawableUBO) == 64);
static_assert(sizeof(FinalTileUBO) == 32);
static_assert(sizeof(EvaluatedUBO) == 176);

template <typename T>
void copyOutput(const T& value, uint8_t* output, size_t outputSize) {
    if (outputSize == sizeof(T)) std::memcpy(output, &value, sizeof(T));
}

int32_t methodValue(std::string_view method) {
    if (method == "combined") return 1;
    if (method == "igor") return 2;
    if (method == "multidirectional") return 3;
    if (method == "basic") return 4;
    return 0;
}

size_t floatArray(const mln_plugin_uniform_context_v1& context,
                  const char* name,
                  float fallback,
                  std::array<float, 4>& output) {
    const auto* value = property(context, name);
    size_t count = 0;
    if (value && value->type == MLN_PLUGIN_VALUE_FLOAT_ARRAY && value->data.float_array_value.data) {
        count = std::min<size_t>(4, value->data.float_array_value.count);
        std::copy_n(value->data.float_array_value.data, count, output.data());
    }
    if (!count) {
        output[0] = fallback;
        count = 1;
    }
    for (size_t i = count; i < output.size(); ++i) output[i] = output[count - 1];
    return count;
}

size_t colorArray(const mln_plugin_uniform_context_v1& context,
                  const char* name,
                  mln_plugin_color fallback,
                  std::array<float, 16>& output) {
    const auto* value = property(context, name);
    size_t count = 0;
    if (value && value->type == MLN_PLUGIN_VALUE_COLOR_ARRAY && value->data.color_array_value.data) {
        count = std::min<size_t>(4, value->data.color_array_value.count);
        for (size_t i = 0; i < count; ++i) {
            const auto& color = value->data.color_array_value.data[i];
            const std::array components{color.r, color.g, color.b, color.a};
            std::copy(components.begin(), components.end(), output.begin() + static_cast<ptrdiff_t>(i * 4));
        }
    }
    if (!count) {
        output[0] = fallback.r;
        output[1] = fallback.g;
        output[2] = fallback.b;
        output[3] = fallback.a;
        count = 1;
    }
    for (size_t i = count; i < 4; ++i) {
        std::copy_n(output.begin() + static_cast<ptrdiff_t>((count - 1) * 4),
                    4,
                    output.begin() + static_cast<ptrdiff_t>(i * 4));
    }
    return count;
}

mln_plugin_status updateUniform(const mln_plugin_uniform_context_v1* context,
                                uint32_t uniformID,
                                uint8_t* output,
                                size_t outputSize) {
    if (!context || context->struct_size < sizeof(*context) || !output) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    if (context->pass_id == 1) {
        if (uniformID == 0 && outputSize == sizeof(PrepareDrawableUBO)) {
            PrepareDrawableUBO value{};
            std::copy_n(context->render_target_matrix, 16, value.matrix);
            copyOutput(value, output, outputSize);
            return MLN_PLUGIN_STATUS_OK;
        }
        if (uniformID == 1 && outputSize == sizeof(PrepareTileUBO)) {
            PrepareTileUBO value{};
            const bool terrarium = context->dem_encoding == MLN_PLUGIN_RASTER_DEM_TERRARIUM;
            const std::array<float, 4> unpack = terrarium
                                                    ? std::array<float, 4>{256.0f, 1.0f, 1.0f / 256.0f, 32768.0f}
                                                    : std::array<float, 4>{6553.6f, 25.6f, 0.1f, 10000.0f};
            std::copy(unpack.begin(), unpack.end(), value.unpack);
            value.dimension[0] = static_cast<float>(context->dem_stride);
            value.dimension[1] = static_cast<float>(context->dem_stride);
            value.zoom = static_cast<float>(context->canonical_z);
            value.maxzoom = static_cast<float>(context->source_max_zoom);
            copyOutput(value, output, outputSize);
            return MLN_PLUGIN_STATUS_OK;
        }
    } else if (context->pass_id == 2) {
        if (uniformID == 0 && outputSize == sizeof(FinalDrawableUBO)) {
            FinalDrawableUBO value{};
            std::copy_n(context->tile_matrix, 16, value.matrix);
            copyOutput(value, output, outputSize);
            return MLN_PLUGIN_STATUS_OK;
        }
        if (uniformID == 1 && outputSize == sizeof(FinalTileUBO)) {
            FinalTileUBO value{};
            value.latitudeRange[0] = context->latitude_range[0];
            value.latitudeRange[1] = context->latitude_range[1];
            value.exaggeration = number(*context, "hillshade-exaggeration", 0.5f);
            value.method = methodValue(stringValue(*context, "hillshade-method", "standard"));
            std::array<float, 4> directions{};
            std::array<float, 4> altitudes{};
            const auto directionCount = floatArray(*context, "hillshade-illumination-direction", 335.0f, directions);
            const auto altitudeCount = floatArray(*context, "hillshade-illumination-altitude", 45.0f, altitudes);
            std::array<float, 16> colors{};
            const auto shadowCount = colorArray(
                *context, "hillshade-shadow-color", {0.0f, 0.0f, 0.0f, 1.0f}, colors);
            const auto highlightCount = colorArray(
                *context, "hillshade-highlight-color", {1.0f, 1.0f, 1.0f, 1.0f}, colors);
            value.numberOfLights = static_cast<int32_t>(
                std::min<size_t>(4, std::max({directionCount, altitudeCount, shadowCount, highlightCount})));
            copyOutput(value, output, outputSize);
            return MLN_PLUGIN_STATUS_OK;
        }
        if (uniformID == 2 && outputSize == sizeof(EvaluatedUBO)) {
            EvaluatedUBO value{};
            if (const auto* accent = property(*context, "hillshade-accent-color");
                accent && accent->type == MLN_PLUGIN_VALUE_COLOR) {
                value.accent[0] = accent->data.color_value.r;
                value.accent[1] = accent->data.color_value.g;
                value.accent[2] = accent->data.color_value.b;
                value.accent[3] = accent->data.color_value.a;
            } else {
                value.accent[3] = 1.0f;
            }
            std::array<float, 4> altitudes{};
            std::array<float, 4> directions{};
            floatArray(*context, "hillshade-illumination-altitude", 45.0f, altitudes);
            floatArray(*context, "hillshade-illumination-direction", 335.0f, directions);
            constexpr float degreesToRadians = 0.01745329251994329577f;
            const bool viewport = stringValue(*context, "hillshade-illumination-anchor", "viewport") == "viewport";
            for (size_t i = 0; i < 4; ++i) {
                value.altitudes[i] = altitudes[i] * degreesToRadians;
                value.azimuths[i] = directions[i] * degreesToRadians -
                                    (viewport ? static_cast<float>(context->bearing) : 0.0f);
            }
            colorArray(*context, "hillshade-shadow-color", {0.0f, 0.0f, 0.0f, 1.0f}, value.shadows);
            colorArray(*context, "hillshade-highlight-color", {1.0f, 1.0f, 1.0f, 1.0f}, value.highlights);
            copyOutput(value, output, outputSize);
            return MLN_PLUGIN_STATUS_OK;
        }
    }
    return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
}

const std::array<mln_plugin_shader_attribute_v1, 2> attributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), 0, 0, str("a_pos"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), 1, 1, str("a_texture_pos"), MLN_PLUGIN_VERTEX_INT16_X2},
}};

const std::array<mln_plugin_shader_texture_v1, 1> textures = {{
    {sizeof(mln_plugin_shader_texture_v1), 0, 0, str("u_image")},
}};

const std::array<mln_plugin_uniform_block_descriptor_v1, 2> prepareUniforms = {{
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     0,
     str("HillshadePrepareDrawableUBO"),
     sizeof(PrepareDrawableUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX,
     MLN_PLUGIN_UNIFORM_SCOPE_DRAWABLE},
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     1,
     str("HillshadePrepareTilePropsUBO"),
     sizeof(PrepareTileUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX | MLN_PLUGIN_SHADER_STAGE_FRAGMENT,
     MLN_PLUGIN_UNIFORM_SCOPE_DRAWABLE},
}};

const std::array<mln_plugin_uniform_block_descriptor_v1, 3> finalUniforms = {{
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     0,
     str("HillshadeDrawableUBO"),
     sizeof(FinalDrawableUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX,
     MLN_PLUGIN_UNIFORM_SCOPE_DRAWABLE},
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     1,
     str("HillshadeTilePropsUBO"),
     sizeof(FinalTileUBO),
     MLN_PLUGIN_SHADER_STAGE_FRAGMENT,
     MLN_PLUGIN_UNIFORM_SCOPE_DRAWABLE},
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     2,
     str("HillshadeEvaluatedPropsUBO"),
     sizeof(EvaluatedUBO),
     MLN_PLUGIN_SHADER_STAGE_FRAGMENT,
     MLN_PLUGIN_UNIFORM_SCOPE_LAYER},
}};

const std::array<mln_plugin_shader_source_v1, 3> prepareSources = {{
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_OPENGL,
     str(shaders::glPrepareVertex, std::char_traits<char>::length(shaders::glPrepareVertex)),
     str(shaders::glPrepareFragment, std::char_traits<char>::length(shaders::glPrepareFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_VULKAN,
     str(shaders::vulkanPrepareVertex, std::char_traits<char>::length(shaders::vulkanPrepareVertex)),
     str(shaders::vulkanPrepareFragment, std::char_traits<char>::length(shaders::vulkanPrepareFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_METAL,
     str(shaders::metalPrepare, std::char_traits<char>::length(shaders::metalPrepare)),
     {},
     str("vertexMain"),
     str("fragmentMain")},
}};

const std::array<mln_plugin_shader_source_v1, 3> finalSources = {{
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_OPENGL,
     str(shaders::glFinalVertex, std::char_traits<char>::length(shaders::glFinalVertex)),
     str(shaders::glFinalFragment, std::char_traits<char>::length(shaders::glFinalFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_VULKAN,
     str(shaders::vulkanFinalVertex, std::char_traits<char>::length(shaders::vulkanFinalVertex)),
     str(shaders::vulkanFinalFragment, std::char_traits<char>::length(shaders::vulkanFinalFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_METAL,
     str(shaders::metalFinal, std::char_traits<char>::length(shaders::metalFinal)),
     {},
     str("vertexMain"),
     str("fragmentMain")},
}};

const std::array<mln_plugin_shader_descriptor_v1, 2> shaderDescriptors = {{
    {sizeof(mln_plugin_shader_descriptor_v1),
     str("hillshade-prepare"),
     prepareSources.data(),
     prepareSources.size(),
     attributes.data(),
     attributes.size(),
     prepareUniforms.data(),
     prepareUniforms.size(),
     textures.data(),
     textures.size()},
    {sizeof(mln_plugin_shader_descriptor_v1),
     str("hillshade"),
     finalSources.data(),
     finalSources.size(),
     attributes.data(),
     attributes.size(),
     finalUniforms.data(),
     finalUniforms.size(),
     textures.data(),
     textures.size()},
}};

const std::array<mln_plugin_render_target_descriptor_v1, 1> renderTargets = {{
    {sizeof(mln_plugin_render_target_descriptor_v1),
     1,
     MLN_PLUGIN_RENDER_TARGET_SOURCE_TILE,
     MLN_PLUGIN_RENDER_TARGET_RGBA8},
}};

const std::array<mln_plugin_texture_binding_v1, 1> prepareTextures = {{
    {sizeof(mln_plugin_texture_binding_v1),
     0,
     MLN_PLUGIN_TEXTURE_SOURCE_RASTER_DEM,
     0,
     MLN_PLUGIN_TEXTURE_FILTER_NEAREST,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP},
}};

const std::array<mln_plugin_texture_binding_v1, 1> finalTextures = {{
    {sizeof(mln_plugin_texture_binding_v1),
     0,
     MLN_PLUGIN_TEXTURE_SOURCE_RENDER_TARGET,
     1,
     MLN_PLUGIN_TEXTURE_FILTER_LINEAR,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP},
}};

const std::array<mln_plugin_render_pass_descriptor_v1, 2> renderPasses = {{
    {sizeof(mln_plugin_render_pass_descriptor_v1),
     1,
     str("hillshade-prepare"),
     MLN_PLUGIN_GRAPH_GEOMETRY_RASTER_DEM_FULL_TILE,
     1,
     MLN_PLUGIN_RENDER_STAGE_PREPARE,
     MLN_PLUGIN_DRAW_MODE_TRIANGLES,
     MLN_PLUGIN_DEPTH_DISABLED,
     MLN_PLUGIN_BLEND_REPLACE,
     0,
     0,
     prepareTextures.data(),
     prepareTextures.size()},
    {sizeof(mln_plugin_render_pass_descriptor_v1),
     2,
     str("hillshade"),
     MLN_PLUGIN_GRAPH_GEOMETRY_RASTER_DEM_MASKED_TILE,
     0,
     MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT,
     MLN_PLUGIN_DRAW_MODE_TRIANGLES,
     MLN_PLUGIN_DEPTH_DISABLED,
     MLN_PLUGIN_BLEND_PREMULTIPLIED_ALPHA,
     0,
     0,
     finalTextures.data(),
     finalTextures.size()},
}};

const mln_plugin_render_graph_v1 renderGraph = {
    sizeof(mln_plugin_render_graph_v1),
    renderTargets.data(),
    renderTargets.size(),
    renderPasses.data(),
    renderPasses.size(),
};

const mln_plugin_layer_type_v1 layerType = [] {
    mln_plugin_layer_type_v1 value{};
    value.struct_size = sizeof(value);
    value.layer_type = str("org.maplibre.hillshade");
    value.backend_mask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN | MLN_PLUGIN_BACKEND_METAL;
    value.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
    value.properties = properties.data();
    value.property_count = properties.size();
    value.source_kind = MLN_PLUGIN_SOURCE_RASTER_DEM;
    value.shaders = shaderDescriptors.data();
    value.shader_count = shaderDescriptors.size();
    value.render_graph = &renderGraph;
    value.update_uniform_block = updateUniform;
    value.participates_in_3d_pass = 1;
    return value;
}();

const mln_plugin_descriptor_v1 descriptor = {
    sizeof(mln_plugin_descriptor_v1),
    MLN_PLUGIN_ABI_VERSION_1,
    str("org.maplibre.hillshade"),
    str(MLN_HILLSHADE_PLUGIN_VERSION, sizeof(MLN_HILLSHADE_PLUGIN_VERSION) - 1),
    MLN_PLUGIN_ABI_VERSION_1,
    MLN_PLUGIN_ABI_VERSION_1,
    nullptr,
    0,
    &layerType,
    1,
};

} // namespace

extern "C" mln_plugin_status mln_hillshade_layer_register(mln_plugin_register_function_v1 registerPlugin,
                                                            char* errorMessage,
                                                            size_t errorMessageCapacity) {
    if (!registerPlugin) return MLN_PLUGIN_STATUS_NOT_FOUND;
    return registerPlugin(&descriptor, errorMessage, errorMessageCapacity);
}
