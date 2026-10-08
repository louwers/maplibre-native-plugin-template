// A minimal MapLibre Native layer plugin: the `square` layer type draws a
// screen-aligned square at every point of a GeoJSON or vector tile source.
//
//   {"id": "dots", "type": "square", "source": "points",
//    "paint": {"square-color": "#e55e5e", "square-size": 24}}
//
// Both paint properties accept constants, zoom expressions and data-driven
// expressions such as ["get", "color"].
//
// A plugin describes itself to MapLibre with plain C structs (see
// <mln/plugin/plugin_api.h>). MapLibre then owns everything GPU-related:
//
// 1. Layout (worker threads): MapLibre hands each point feature of a tile to
//    layoutFeature(). We turn every point into four vertices and two triangles.
// 2. Paint properties: MapLibre evaluates the style expressions and writes the
//    results into our uniform block (constant/zoom values) or into per-vertex
//    attributes (data-driven values), as declared by the property bindings.
// 3. Rendering (render thread): every frame, updateUniform() fills the rest of
//    the uniform block (the tile matrix) and MapLibre draws the triangles with
//    our shaders on OpenGL, Vulkan or Metal.

#include "square_layer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

// The build tools define MLN_PLUGIN_VERSION from plugin.json.
#ifndef MLN_PLUGIN_VERSION
#define MLN_PLUGIN_VERSION "0.0.0-dev"
#endif

namespace {

constexpr mln_plugin_string str(const char* value, size_t size) { return {value, size}; }

template <size_t N>
constexpr mln_plugin_string str(const char (&value)[N]) {
    return str(value, N - 1);
}

// ---------------------------------------------------------------------------
// Geometry produced by layout
// ---------------------------------------------------------------------------

// Shader attribute IDs. Attributes 2-4 only exist when a property is data-driven.
constexpr uint32_t positionAttribute = 0;
constexpr uint32_t cornerAttribute = 1;
constexpr uint32_t sizeAttribute = 2;         // float2: size at the lower and upper zoom stop
constexpr uint32_t colorMinimumAttribute = 3; // color at the lower zoom stop
constexpr uint32_t colorMaximumAttribute = 4; // color at the upper zoom stop

constexpr uint32_t vertexStream = 0;
constexpr uint64_t squareDrawable = 1;

// Every corner of a square stores the point's tile coordinate and which corner
// it is (-1 or 1 in x and y). The vertex shader moves the corner outwards by
// half the square size in screen pixels.
struct Vertex {
    int16_t position[2];
    int16_t corner[2];
};
static_assert(sizeof(Vertex) == 8);

struct Layout {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    // Indices are 16-bit, so vertices are split into segments of at most 65536.
    std::vector<mln_plugin_segment_v1> segments;
    // Which vertices belong to which feature, so MapLibre can write data-driven
    // paint values for each feature.
    std::vector<mln_plugin_feature_vertex_range_v1> featureRanges;
    // Output views; they must stay valid until destroyLayout().
    mln_plugin_vertex_stream_v1 stream{};
    std::array<mln_plugin_attribute_binding_v1, 2> attributes{};
    mln_plugin_drawable_descriptor_v1 drawable{};
};

void startSegment(Layout& layout) {
    mln_plugin_segment_v1 segment{};
    segment.struct_size = sizeof(segment);
    segment.vertex_offset = static_cast<uint32_t>(layout.vertices.size());
    segment.index_offset = static_cast<uint32_t>(layout.indices.size());
    layout.segments.push_back(segment);
}

mln_plugin_status createLayout(const mln_plugin_layout_context_v1* context, void** instance) {
    if (!context || context->struct_size < sizeof(*context) || !instance) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    auto* layout = new (std::nothrow) Layout();
    if (!layout) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    startSegment(*layout);
    *instance = layout;
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status layoutFeature(void* instance, const mln_plugin_feature_v1* feature) {
    if (!instance || !feature || feature->struct_size < sizeof(*feature) ||
        feature->geometry_type != MLN_PLUGIN_GEOMETRY_POINT || (feature->point_count && !feature->points)) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto& layout = *static_cast<Layout*>(instance);
    const auto firstVertex = static_cast<uint32_t>(layout.vertices.size());

    // A MultiPoint feature has several points; each gets its own square.
    for (size_t i = 0; i < feature->point_count; ++i) {
        if (layout.segments.back().vertex_length > std::numeric_limits<uint16_t>::max() - 4u) {
            startSegment(layout);
        }
        auto& segment = layout.segments.back();
        const auto base = static_cast<uint16_t>(segment.vertex_length);
        const auto point = feature->points[i];
        constexpr int16_t corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const auto& corner : corners) {
            layout.vertices.push_back({{point.x, point.y}, {corner[0], corner[1]}});
        }
        const uint16_t triangles[] = {base,
                                      static_cast<uint16_t>(base + 1),
                                      static_cast<uint16_t>(base + 2),
                                      base,
                                      static_cast<uint16_t>(base + 2),
                                      static_cast<uint16_t>(base + 3)};
        layout.indices.insert(layout.indices.end(), std::begin(triangles), std::end(triangles));
        segment.vertex_length += 4;
        segment.index_length += 6;
    }

    if (layout.vertices.size() > firstVertex) {
        layout.featureRanges.push_back({sizeof(mln_plugin_feature_vertex_range_v1),
                                        feature->feature_index,
                                        squareDrawable,
                                        firstVertex,
                                        static_cast<uint32_t>(layout.vertices.size() - firstVertex)});
    }
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status finishLayout(void* instance, mln_plugin_bucket_v1* bucket) {
    if (!instance || !bucket || bucket->struct_size < sizeof(*bucket)) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    auto& layout = *static_cast<Layout*>(instance);
    if (layout.indices.empty()) return MLN_PLUGIN_STATUS_OK; // nothing to draw in this tile
    if (layout.segments.back().index_length == 0) layout.segments.pop_back();

    layout.stream = {sizeof(mln_plugin_vertex_stream_v1),
                     vertexStream,
                     reinterpret_cast<const uint8_t*>(layout.vertices.data()),
                     layout.vertices.size() * sizeof(Vertex),
                     static_cast<uint32_t>(layout.vertices.size()),
                     sizeof(Vertex)};
    layout.attributes = {{
        {sizeof(mln_plugin_attribute_binding_v1), positionAttribute, vertexStream, offsetof(Vertex, position)},
        {sizeof(mln_plugin_attribute_binding_v1), cornerAttribute, vertexStream, offsetof(Vertex, corner)},
    }};

    // One drawable with every square of the tile, drawn with the "square" shader
    // as indexed triangles in the translucent pass.
    auto& drawable = layout.drawable;
    drawable.struct_size = sizeof(drawable);
    drawable.drawable_key = squareDrawable;
    drawable.shader_id = str("square");
    drawable.depth_mode = MLN_PLUGIN_DRAWABLE_DEPTH_READ_ONLY;
    drawable.attributes = layout.attributes.data();
    drawable.attribute_count = layout.attributes.size();
    drawable.segments = layout.segments.data();
    drawable.segment_count = layout.segments.size();

    bucket->vertex_streams = &layout.stream;
    bucket->vertex_stream_count = 1;
    bucket->indices = layout.indices.data();
    bucket->index_count = layout.indices.size();
    bucket->drawables = &layout.drawable;
    bucket->drawable_count = 1;
    bucket->feature_vertex_ranges = layout.featureRanges.data();
    bucket->feature_vertex_range_count = layout.featureRanges.size();
    return MLN_PLUGIN_STATUS_OK;
}

void destroyLayout(void* instance) { delete static_cast<Layout*>(instance); }

// ---------------------------------------------------------------------------
// Uniform block
// ---------------------------------------------------------------------------

// Mirrors `SquareUBO` in the shaders below (std140 layout, 16-byte aligned).
// updateUniform() writes the matrix and extrude scale; MapLibre writes the
// paint properties and their zoom interpolation factors (see propertyBindings).
struct alignas(16) SquareUBO {
    float matrix[16];      // tile coordinates to clip space
    float extrudeScale[2]; // screen pixels to clip space
    float size;            // square-size, when it is not data-driven
    float sizeT;           // interpolation factor between the two size stops
    float color[4];        // square-color (premultiplied), when it is not data-driven
    float colorT;          // interpolation factor between the two color stops
    float padding[3];
};
static_assert(offsetof(SquareUBO, extrudeScale) == 64);
static_assert(offsetof(SquareUBO, color) == 80);
static_assert(offsetof(SquareUBO, colorT) == 96);
static_assert(sizeof(SquareUBO) == 112);

mln_plugin_status updateUniform(const mln_plugin_uniform_context_v1* context,
                                uint32_t uniformID,
                                uint8_t* output,
                                size_t outputSize) {
    if (!context || context->struct_size < sizeof(*context) || !output || uniformID != 0 ||
        outputSize != sizeof(SquareUBO)) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    // Only write our own fields; MapLibre owns the property ranges of the block.
    std::memcpy(output + offsetof(SquareUBO, matrix), context->tile_matrix, sizeof(SquareUBO::matrix));
    std::memcpy(output + offsetof(SquareUBO, extrudeScale), context->pixels_to_gl_units, sizeof(float) * 2);
    return MLN_PLUGIN_STATUS_OK;
}

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
//
// MapLibre prepends a prelude to every shader. It defines
// MLN_PLUGIN_UNIFORM_0_BINDING and, for every bound property, a macro such as
// MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM: 1 when the value comes from the
// uniform block, 0 when it comes from per-vertex attributes (data-driven).

constexpr char openglVertex[] = R"SHADER(
layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_corner;
#if !MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM
layout (location = 2) in vec2 a_size;
#endif
#if !MLN_PLUGIN_PROPERTY_SQUARE_COLOR_IS_UNIFORM
layout (location = 3) in vec4 a_color_min;
layout (location = 4) in vec4 a_color_max;
#endif

layout (std140) uniform SquareUBO {
    mat4 u_matrix;
    vec2 u_extrude_scale;
    float u_size;
    float u_size_factor;
    vec4 u_color;
    float u_color_factor;
};

out vec4 v_color;

void main() {
    float size = u_size;
    vec4 color = u_color;
#if !MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM
    size = mix(a_size.x, a_size.y, u_size_factor);
#endif
#if !MLN_PLUGIN_PROPERTY_SQUARE_COLOR_IS_UNIFORM
    color = mix(a_color_min, a_color_max, u_color_factor);
#endif
    gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);
    // Multiplying by w keeps the square the same size on screen when pitched.
    gl_Position.xy += a_corner * 0.5 * max(size, 0.0) * u_extrude_scale * gl_Position.w;
    v_color = color;
}
)SHADER";

constexpr char openglFragment[] = R"SHADER(
in vec4 v_color;

void main() {
    fragColor = v_color;
}
)SHADER";

constexpr char vulkanVertex[] = R"SHADER(
layout(location = 0) in ivec2 a_position;
layout(location = 1) in ivec2 a_corner;
#if !MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM
layout(location = 2) in vec2 a_size;
#endif
#if !MLN_PLUGIN_PROPERTY_SQUARE_COLOR_IS_UNIFORM
layout(location = 3) in vec4 a_color_min;
layout(location = 4) in vec4 a_color_max;
#endif

layout(std140, set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform SquareUBO {
    mat4 matrix;
    vec2 extrude_scale;
    float size;
    float size_factor;
    vec4 color;
    float color_factor;
} square;

layout(location = 0) out vec4 v_color;

void main() {
    float size = square.size;
    vec4 color = square.color;
#if !MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM
    size = mix(a_size.x, a_size.y, square.size_factor);
#endif
#if !MLN_PLUGIN_PROPERTY_SQUARE_COLOR_IS_UNIFORM
    color = mix(a_color_min, a_color_max, square.color_factor);
#endif
    gl_Position = square.matrix * vec4(vec2(a_position), 0.0, 1.0);
    gl_Position.xy += vec2(a_corner) * 0.5 * max(size, 0.0) * square.extrude_scale * gl_Position.w;
    applySurfaceTransform();
    v_color = color;
}
)SHADER";

constexpr char vulkanFragment[] = R"SHADER(
layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = v_color;
}
)SHADER";

// Metal takes one source with both entry points.
constexpr char metalSource[] = R"SHADER(
struct alignas(16) SquareUBO {
    float4x4 matrix;
    float2 extrude_scale;
    float size;
    float size_factor;
    float4 color;
    float color_factor;
    float padding0;
    float padding1;
    float padding2;
};

struct SquareVertex {
    short2 position [[attribute(0)]];
    short2 corner [[attribute(1)]];
#if !MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM
    float2 size [[attribute(2)]];
#endif
#if !MLN_PLUGIN_PROPERTY_SQUARE_COLOR_IS_UNIFORM
    float4 color_min [[attribute(3)]];
    float4 color_max [[attribute(4)]];
#endif
};

struct SquareFragment {
    float4 position [[position, invariant]];
    float4 color;
};

SquareFragment vertex squareVertex(thread const SquareVertex vertx [[stage_in]],
                                   device const SquareUBO& square [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {
    float size = square.size;
    float4 color = square.color;
#if !MLN_PLUGIN_PROPERTY_SQUARE_SIZE_IS_UNIFORM
    size = mix(vertx.size.x, vertx.size.y, square.size_factor);
#endif
#if !MLN_PLUGIN_PROPERTY_SQUARE_COLOR_IS_UNIFORM
    color = mix(vertx.color_min, vertx.color_max, square.color_factor);
#endif
    float4 position = square.matrix * float4(float2(vertx.position), 0.0, 1.0);
    position.xy += float2(vertx.corner) * 0.5 * max(size, 0.0) * square.extrude_scale * position.w;
    return {position, color};
}

half4 fragment squareFragment(SquareFragment in [[stage_in]]) {
    return half4(in.color);
}
)SHADER";

// ---------------------------------------------------------------------------
// Registration metadata
// ---------------------------------------------------------------------------

constexpr mln_plugin_value makeFloat(float value) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_FLOAT;
    result.data.float_value = value;
    return result;
}

constexpr mln_plugin_value makeColor(float r, float g, float b, float a) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_COLOR;
    result.data.color_value = {r, g, b, a};
    return result;
}

constexpr uint32_t anyExpression = MLN_PLUGIN_EXPRESSION_CAMERA | MLN_PLUGIN_EXPRESSION_FEATURE |
                                   MLN_PLUGIN_EXPRESSION_COMPOSITE;

mln_plugin_property_descriptor_v1 property(mln_plugin_string name, mln_plugin_value_type type, mln_plugin_value value) {
    mln_plugin_property_descriptor_v1 result{};
    result.struct_size = sizeof(result);
    result.name = name;
    result.type = type;
    result.default_value = value;
    result.expression_capabilities = anyExpression;
    result.supports_transitions = 1;
    return result;
}

// The paint properties a style can set on a `square` layer, with their defaults.
const std::array<mln_plugin_property_descriptor_v1, 2> properties = {{
    property(str("square-color"), MLN_PLUGIN_VALUE_COLOR, makeColor(0, 0, 0, 1)),
    property(str("square-size"), MLN_PLUGIN_VALUE_FLOAT, makeFloat(16)),
}};

// Vertex attributes in location order. Locations must start at 0 and be contiguous.
const std::array<mln_plugin_shader_attribute_v1, 5> attributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), positionAttribute, 0, str("a_position"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), cornerAttribute, 1, str("a_corner"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), sizeAttribute, 2, str("a_size"), MLN_PLUGIN_VERTEX_FLOAT_X2},
    {sizeof(mln_plugin_shader_attribute_v1), colorMinimumAttribute, 3, str("a_color_min"), MLN_PLUGIN_VERTEX_FLOAT_X4},
    {sizeof(mln_plugin_shader_attribute_v1), colorMaximumAttribute, 4, str("a_color_max"), MLN_PLUGIN_VERTEX_FLOAT_X4},
}};

const std::array<mln_plugin_uniform_block_descriptor_v1, 1> uniformBlocks = {{
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     0,
     str("SquareUBO"),
     sizeof(SquareUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX,
     MLN_PLUGIN_UNIFORM_DRAWABLE},
}};

// Tell MapLibre where each paint property goes: into the uniform block when it
// is constant or zoom-dependent, otherwise into the vertex attributes. Using the
// same attribute ID for minimum and maximum packs both stops into one float2.
const std::array<mln_plugin_shader_property_binding_v1, 2> propertyBindings = {{
    {sizeof(mln_plugin_shader_property_binding_v1),
     str("square-size"),
     MLN_PLUGIN_PROPERTY_ENCODING_FLOAT,
     0,
     offsetof(SquareUBO, size),
     sizeAttribute,
     sizeAttribute,
     0,
     offsetof(SquareUBO, sizeT)},
    {sizeof(mln_plugin_shader_property_binding_v1),
     str("square-color"),
     MLN_PLUGIN_PROPERTY_ENCODING_COLOR,
     0,
     offsetof(SquareUBO, color),
     colorMinimumAttribute,
     colorMaximumAttribute,
     0,
     offsetof(SquareUBO, colorT)},
}};

const std::array<mln_plugin_shader_source_v1, 3> shaderSources = {{
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_OPENGL, str(openglVertex), str(openglFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_VULKAN, str(vulkanVertex), str(vulkanFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_METAL,
     str(metalSource),
     {},
     str("squareVertex"),
     str("squareFragment")},
}};

const mln_plugin_shader_descriptor_v1 shader = {sizeof(mln_plugin_shader_descriptor_v1),
                                                str("square"),
                                                shaderSources.data(),
                                                shaderSources.size(),
                                                attributes.data(),
                                                attributes.size(),
                                                uniformBlocks.data(),
                                                uniformBlocks.size(),
                                                propertyBindings.data(),
                                                propertyBindings.size()};

const mln_plugin_layer_type_v1 layerType = [] {
    mln_plugin_layer_type_v1 value{};
    value.struct_size = sizeof(value);
    value.layer_type = str("square"); // the "type" used in style JSON
    value.backend_mask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN | MLN_PLUGIN_BACKEND_METAL;
    value.properties = properties.data();
    value.property_count = properties.size();
    value.geometry_type_mask = MLN_PLUGIN_GEOMETRY_POINT;
    value.shaders = &shader;
    value.shader_count = 1;
    value.create_layout = createLayout;
    value.layout_feature = layoutFeature;
    value.finish_layout = finishLayout;
    value.destroy_layout = destroyLayout;
    value.update_uniform_block = updateUniform;
    // query_feature and get_query_radius are optional; without them the layer
    // is not returned by queryRenderedFeatures.
    return value;
}();

const mln_plugin_descriptor_v1 descriptor = {sizeof(mln_plugin_descriptor_v1),
                                             MLN_PLUGIN_ABI_VERSION_1,
                                             str("org.maplibre.square-layer"),
                                             str(MLN_PLUGIN_VERSION),
                                             MLN_PLUGIN_ABI_VERSION_1,
                                             MLN_PLUGIN_ABI_VERSION_1,
                                             &layerType,
                                             1};

} // namespace

extern "C" mln_plugin_status mln_square_layer_register(mln_plugin_register_function_v1 registerPlugin,
                                                         char* errorMessage,
                                                         size_t errorMessageCapacity) {
    if (!registerPlugin) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    // MapLibre copies the descriptor; only the callback addresses are retained.
    return registerPlugin(&descriptor, errorMessage, errorMessageCapacity);
}
