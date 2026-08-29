#include "rectangle_layer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

#ifndef MLN_RECTANGLE_PLUGIN_VERSION
#define MLN_RECTANGLE_PLUGIN_VERSION "0.1.0-local"
#endif

namespace {

constexpr uint32_t positionAttribute = 0;
constexpr uint32_t cornerAttribute = 1;
constexpr uint32_t sizeAttribute = 2;
constexpr uint32_t colorAttribute = 3;
constexpr uint32_t strokeWidthAttribute = 4;
constexpr uint32_t strokeColorAttribute = 5;
constexpr uint32_t vertexStream = 0;

constexpr mln_plugin_string str(const char* value, size_t size) { return {value, size}; }

template <size_t N>
constexpr mln_plugin_string str(const char (&value)[N]) {
    return str(value, N - 1);
}

struct Vertex {
    int16_t position[2];
    int16_t corner[2];
    float size[2];
    float color[4];
    float strokeWidth;
    float strokeColor[4];
};

static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, corner) == 4);
static_assert(offsetof(Vertex, size) == 8);
static_assert(offsetof(Vertex, color) == 16);
static_assert(offsetof(Vertex, strokeWidth) == 32);
static_assert(offsetof(Vertex, strokeColor) == 36);
static_assert(sizeof(Vertex) == 52);

struct Layout {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<mln_plugin_segment_v1> segments;
    std::array<mln_plugin_vertex_stream_v1, 1> streams{};
    std::array<mln_plugin_attribute_binding_v1, 6> attributes{};
    std::array<mln_plugin_drawable_descriptor_v1, 1> drawables{};
    float queryRadius = 0.0f;
};

const mln_plugin_value* property(const mln_plugin_feature_v1& feature, const char* name) {
    const auto length = std::strlen(name);
    for (size_t i = 0; i < feature.evaluated_property_count; ++i) {
        const auto& candidate = feature.evaluated_properties[i];
        if (candidate.name.size == length && candidate.name.data &&
            std::memcmp(candidate.name.data, name, length) == 0) {
            return &candidate.value;
        }
    }
    return nullptr;
}

float number(const mln_plugin_feature_v1& feature, const char* name, float fallback) {
    const auto* value = property(feature, name);
    return value && value->type == MLN_PLUGIN_VALUE_FLOAT && std::isfinite(value->data.float_value)
               ? value->data.float_value
               : fallback;
}

std::array<float, 4> color(const mln_plugin_feature_v1& feature,
                           const char* name,
                           std::array<float, 4> fallback) {
    const auto* value = property(feature, name);
    if (!value || value->type != MLN_PLUGIN_VALUE_COLOR) return fallback;
    return {value->data.color_value.r,
            value->data.color_value.g,
            value->data.color_value.b,
            value->data.color_value.a};
}

void startSegment(Layout& layout) {
    mln_plugin_segment_v1 segment{};
    segment.struct_size = sizeof(segment);
    segment.vertex_offset = static_cast<uint32_t>(layout.vertices.size());
    segment.index_offset = static_cast<uint32_t>(layout.indices.size());
    layout.segments.push_back(segment);
}

mln_plugin_status createLayout(const mln_plugin_layout_context_v1* context, void** instance) {
    if (!context || context->struct_size < sizeof(*context) || !instance) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto* layout = new (std::nothrow) Layout();
    if (!layout) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    startSegment(*layout);
    *instance = layout;
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status layoutFeature(void* instance, const mln_plugin_feature_v1* feature) {
    if (!instance || !feature || feature->struct_size < sizeof(*feature) ||
        feature->geometry_type != MLN_PLUGIN_GEOMETRY_POINT || !feature->points) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto& layout = *static_cast<Layout*>(instance);
    const float width = std::max(0.0f, number(*feature, "rectangle-width", 10.0f));
    const float height = std::max(0.0f, number(*feature, "rectangle-height", 10.0f));
    const float strokeWidth = std::clamp(number(*feature, "rectangle-stroke-width", 0.0f),
                                         0.0f,
                                         0.5f * std::min(width, height));
    const auto fill = color(*feature, "rectangle-color", {0.0f, 0.0f, 0.0f, 1.0f});
    const auto stroke = color(*feature, "rectangle-stroke-color", {0.0f, 0.0f, 0.0f, 1.0f});
    layout.queryRadius = std::max(layout.queryRadius, 0.5f * std::max(width, height));

    for (size_t pointIndex = 0; pointIndex < feature->point_count; ++pointIndex) {
        auto& segment = layout.segments.back();
        if (segment.vertex_length > std::numeric_limits<uint16_t>::max() - 4u) {
            startSegment(layout);
        }
        auto& active = layout.segments.back();
        const uint16_t base = static_cast<uint16_t>(active.vertex_length);
        const auto point = feature->points[pointIndex];
        constexpr int16_t corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const auto& corner : corners) {
            Vertex vertex{};
            vertex.position[0] = point.x;
            vertex.position[1] = point.y;
            vertex.corner[0] = corner[0];
            vertex.corner[1] = corner[1];
            vertex.size[0] = width;
            vertex.size[1] = height;
            std::copy(fill.begin(), fill.end(), vertex.color);
            vertex.strokeWidth = strokeWidth;
            std::copy(stroke.begin(), stroke.end(), vertex.strokeColor);
            layout.vertices.push_back(vertex);
        }
        const uint16_t quad[] = {base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                                 base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3)};
        layout.indices.insert(layout.indices.end(), std::begin(quad), std::end(quad));
        active.vertex_length += 4;
        active.index_length += 6;
        active.feature_index = feature->feature_index;
    }
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status finishLayout(void* instance, mln_plugin_bucket_v1* output) {
    if (!instance || !output || output->struct_size < sizeof(*output)) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto& layout = *static_cast<Layout*>(instance);
    layout.segments.erase(std::remove_if(layout.segments.begin(),
                                         layout.segments.end(),
                                         [](const auto& segment) { return segment.index_length == 0; }),
                          layout.segments.end());
    layout.streams[0] = {sizeof(mln_plugin_vertex_stream_v1),
                         vertexStream,
                         reinterpret_cast<const uint8_t*>(layout.vertices.data()),
                         layout.vertices.size() * sizeof(Vertex),
                         static_cast<uint32_t>(layout.vertices.size()),
                         sizeof(Vertex)};
    layout.attributes = {{
        {sizeof(mln_plugin_attribute_binding_v1), positionAttribute, vertexStream, offsetof(Vertex, position), MLN_PLUGIN_VERTEX_INT16_X2},
        {sizeof(mln_plugin_attribute_binding_v1), cornerAttribute, vertexStream, offsetof(Vertex, corner), MLN_PLUGIN_VERTEX_INT16_X2},
        {sizeof(mln_plugin_attribute_binding_v1), sizeAttribute, vertexStream, offsetof(Vertex, size), MLN_PLUGIN_VERTEX_FLOAT_X2},
        {sizeof(mln_plugin_attribute_binding_v1), colorAttribute, vertexStream, offsetof(Vertex, color), MLN_PLUGIN_VERTEX_FLOAT_X4},
        {sizeof(mln_plugin_attribute_binding_v1), strokeWidthAttribute, vertexStream, offsetof(Vertex, strokeWidth), MLN_PLUGIN_VERTEX_FLOAT},
        {sizeof(mln_plugin_attribute_binding_v1), strokeColorAttribute, vertexStream, offsetof(Vertex, strokeColor), MLN_PLUGIN_VERTEX_FLOAT_X4},
    }};
    auto& drawable = layout.drawables[0];
    drawable.struct_size = sizeof(drawable);
    drawable.drawable_key = 1;
    drawable.shader_id = str("rectangle");
    drawable.draw_mode = MLN_PLUGIN_DRAW_MODE_TRIANGLES;
    drawable.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
    drawable.depth_mode = MLN_PLUGIN_DEPTH_READ_ONLY;
    drawable.blend_mode = MLN_PLUGIN_BLEND_PREMULTIPLIED_ALPHA;
    drawable.enable_stencil = 1;
    drawable.attributes = layout.attributes.data();
    drawable.attribute_count = layout.attributes.size();
    drawable.segments = layout.segments.data();
    drawable.segment_count = layout.segments.size();

    output->vertex_streams = layout.vertices.empty() ? nullptr : layout.streams.data();
    output->vertex_stream_count = layout.vertices.empty() ? 0 : layout.streams.size();
    output->indices = layout.indices.data();
    output->index_count = layout.indices.size();
    output->drawables = layout.indices.empty() ? nullptr : layout.drawables.data();
    output->drawable_count = layout.indices.empty() ? 0 : layout.drawables.size();
    output->query_radius = layout.queryRadius;
    return MLN_PLUGIN_STATUS_OK;
}

void destroyLayout(void* instance) { delete static_cast<Layout*>(instance); }

bool pointInPolygon(double x, double y, const mln_plugin_tile_point_v1* polygon, size_t count) {
    if (count < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const double xi = polygon[i].x;
        const double yi = polygon[i].y;
        const double xj = polygon[j].x;
        const double yj = polygon[j].y;
        if (((yi > y) != (yj > y)) &&
            x < (xj - xi) * (y - yi) / ((yj - yi) == 0.0 ? 1.0 : (yj - yi)) + xi) {
            inside = !inside;
        }
    }
    return inside;
}

bool segmentIntersectsRectangle(double x0,
                                double y0,
                                double x1,
                                double y1,
                                double minX,
                                double minY,
                                double maxX,
                                double maxY) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double p[] = {-dx, dx, -dy, dy};
    const double q[] = {x0 - minX, maxX - x0, y0 - minY, maxY - y0};
    double lower = 0.0;
    double upper = 1.0;
    for (size_t i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0) return false;
            continue;
        }
        const double ratio = q[i] / p[i];
        if (p[i] < 0.0) {
            lower = std::max(lower, ratio);
        } else {
            upper = std::min(upper, ratio);
        }
        if (lower > upper) return false;
    }
    return true;
}

uint8_t queryFeature(const mln_plugin_feature_v1* feature,
                     const mln_plugin_tile_point_v1* query,
                     size_t queryCount,
                     double pixelsToTileUnits,
                     const mln_plugin_property_value_v1* properties,
                     size_t propertyCount) {
    if (!feature || !feature->points || !query || !queryCount) return 0;
    float width = 10.0f;
    float height = 10.0f;
    for (size_t i = 0; i < propertyCount; ++i) {
        const auto& candidate = properties[i];
        if (!candidate.name.data || candidate.value.type != MLN_PLUGIN_VALUE_FLOAT) continue;
        const std::string name(candidate.name.data, candidate.name.size);
        if (name == "rectangle-width") width = candidate.value.data.float_value;
        if (name == "rectangle-height") height = candidate.value.data.float_value;
    }
    const double halfWidth = std::max(0.0f, width) * 0.5 * pixelsToTileUnits;
    const double halfHeight = std::max(0.0f, height) * 0.5 * pixelsToTileUnits;
    for (size_t pointIndex = 0; pointIndex < feature->point_count; ++pointIndex) {
        const auto point = feature->points[pointIndex];
        const double minX = point.x - halfWidth;
        const double maxX = point.x + halfWidth;
        const double minY = point.y - halfHeight;
        const double maxY = point.y + halfHeight;
        for (size_t queryIndex = 0; queryIndex < queryCount; ++queryIndex) {
            const auto& current = query[queryIndex];
            if ((current.x >= minX && current.x <= maxX && current.y >= minY && current.y <= maxY) ||
                segmentIntersectsRectangle(current.x,
                                           current.y,
                                           query[(queryIndex + 1) % queryCount].x,
                                           query[(queryIndex + 1) % queryCount].y,
                                           minX,
                                           minY,
                                           maxX,
                                           maxY)) {
                return 1;
            }
        }
        if (pointInPolygon(point.x, point.y, query, queryCount) ||
            pointInPolygon(minX, minY, query, queryCount) ||
            pointInPolygon(maxX, minY, query, queryCount) ||
            pointInPolygon(maxX, maxY, query, queryCount) ||
            pointInPolygon(minX, maxY, query, queryCount)) {
            return 1;
        }
    }
    return 0;
}

constexpr char glVertex[] = R"SHADER(
layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_corner;
layout (location = 2) in vec2 a_size;
layout (location = 3) in vec4 a_color;
layout (location = 4) in float a_stroke_width;
layout (location = 5) in vec4 a_stroke_color;

layout (std140) uniform PluginDrawableUBO {
    mat4 u_matrix;
    vec2 u_extrude_scale;
    vec2 u_pad;
};

out vec2 v_corner;
out vec2 v_size;
out vec4 v_color;
out float v_stroke_width;
out vec4 v_stroke_color;

void main() {
    gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);
    gl_Position.xy += a_corner * a_size * 0.5 * u_extrude_scale * gl_Position.w;
    v_corner = a_corner;
    v_size = a_size;
    v_color = a_color;
    v_stroke_width = a_stroke_width;
    v_stroke_color = a_stroke_color;
}
)SHADER";

constexpr char glFragment[] = R"SHADER(
in vec2 v_corner;
in vec2 v_size;
in vec4 v_color;
in float v_stroke_width;
in vec4 v_stroke_color;

void main() {
    vec2 edge_distance = (vec2(1.0) - abs(v_corner)) * v_size * 0.5;
    fragColor = min(edge_distance.x, edge_distance.y) < v_stroke_width ? v_stroke_color : v_color;
}
)SHADER";

constexpr char vkVertex[] = R"SHADER(
layout(location = 0) in ivec2 a_position;
layout(location = 1) in ivec2 a_corner;
layout(location = 2) in vec2 a_size;
layout(location = 3) in vec4 a_color;
layout(location = 4) in float a_stroke_width;
layout(location = 5) in vec4 a_stroke_color;

layout(push_constant) uniform Constants { int ubo_index; } constant;
struct PluginDrawableUBO { mat4 matrix; vec2 extrude_scale; vec2 pad; };
layout(std140, set = LAYER_SET_INDEX, binding = idDrawableReservedVertexOnlyUBO) readonly buffer PluginDrawableUBOVector {
    PluginDrawableUBO values[];
} drawables;

layout(location = 0) out vec2 v_corner;
layout(location = 1) out vec2 v_size;
layout(location = 2) out vec4 v_color;
layout(location = 3) out float v_stroke_width;
layout(location = 4) out vec4 v_stroke_color;

void main() {
    PluginDrawableUBO drawable = drawables.values[constant.ubo_index];
    gl_Position = drawable.matrix * vec4(a_position, 0.0, 1.0);
    gl_Position.xy += vec2(a_corner) * a_size * 0.5 * drawable.extrude_scale * gl_Position.w;
    applySurfaceTransform();
    v_corner = vec2(a_corner);
    v_size = a_size;
    v_color = a_color;
    v_stroke_width = a_stroke_width;
    v_stroke_color = a_stroke_color;
}
)SHADER";

constexpr char vkFragment[] = R"SHADER(
layout(location = 0) in vec2 v_corner;
layout(location = 1) in vec2 v_size;
layout(location = 2) in vec4 v_color;
layout(location = 3) in float v_stroke_width;
layout(location = 4) in vec4 v_stroke_color;
layout(location = 0) out vec4 fragColor;

void main() {
    vec2 edge_distance = (vec2(1.0) - abs(v_corner)) * v_size * 0.5;
    fragColor = min(edge_distance.x, edge_distance.y) < v_stroke_width ? v_stroke_color : v_color;
}
)SHADER";

constexpr char metalSource[] = R"SHADER(
struct alignas(16) PluginDrawableUBO {
    float4x4 matrix;
    float2 extrude_scale;
    float2 pad;
};

struct RectangleVertex {
    short2 position [[attribute(0)]];
    short2 corner [[attribute(1)]];
    float2 size [[attribute(2)]];
    float4 color [[attribute(3)]];
    float stroke_width [[attribute(4)]];
    float4 stroke_color [[attribute(5)]];
};

struct RectangleFragment {
    float4 position [[position, invariant]];
    float2 corner;
    float2 size;
    float4 color;
    float stroke_width;
    float4 stroke_color;
};

vertex RectangleFragment rectangleVertex(
    RectangleVertex vertex [[stage_in]],
    device const uint32_t& uboIndex [[buffer(idGlobalUBOIndex)]],
    device const PluginDrawableUBO* drawables [[buffer(idDrawableReservedVertexOnlyUBO)]]) {
    device const PluginDrawableUBO& drawable = drawables[uboIndex];
    float4 position = drawable.matrix * float4(float2(vertex.position), 0.0, 1.0);
    position.xy += float2(vertex.corner) * vertex.size * 0.5 * drawable.extrude_scale * position.w;
    return {position, float2(vertex.corner), vertex.size, vertex.color, vertex.stroke_width, vertex.stroke_color};
}

fragment half4 rectangleFragment(RectangleFragment in [[stage_in]]) {
    float2 edgeDistance = (float2(1.0) - abs(in.corner)) * in.size * 0.5;
    return half4(min(edgeDistance.x, edgeDistance.y) < in.stroke_width ? in.stroke_color : in.color);
}
)SHADER";

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

const std::array<mln_plugin_property_descriptor_v1, 5> properties = {{
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-color"), MLN_PLUGIN_VALUE_COLOR, MLN_PLUGIN_PROPERTY_PAINT, makeColor(0, 0, 0, 1), 1, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-width"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_PAINT, makeFloat(10), 1, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-height"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_PAINT, makeFloat(10), 1, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-stroke-width"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_PAINT, makeFloat(0), 1, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-stroke-color"), MLN_PLUGIN_VALUE_COLOR, MLN_PLUGIN_PROPERTY_PAINT, makeColor(0, 0, 0, 1), 1, 0},
}};

const std::array<mln_plugin_shader_attribute_v1, 6> shaderAttributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), positionAttribute, 0, str("a_position"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), cornerAttribute, 1, str("a_corner"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), sizeAttribute, 2, str("a_size"), MLN_PLUGIN_VERTEX_FLOAT_X2},
    {sizeof(mln_plugin_shader_attribute_v1), colorAttribute, 3, str("a_color"), MLN_PLUGIN_VERTEX_FLOAT_X4},
    {sizeof(mln_plugin_shader_attribute_v1), strokeWidthAttribute, 4, str("a_stroke_width"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), strokeColorAttribute, 5, str("a_stroke_color"), MLN_PLUGIN_VERTEX_FLOAT_X4},
}};

const std::array<mln_plugin_shader_source_v1, 3> shaderSources = {{
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_OPENGL, str(glVertex), str(glFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_VULKAN, str(vkVertex), str(vkFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_METAL, str(metalSource), {}, str("rectangleVertex"), str("rectangleFragment")},
}};

const std::array<mln_plugin_shader_descriptor_v1, 1> shaders = {{
    {sizeof(mln_plugin_shader_descriptor_v1), str("rectangle"), shaderSources.data(), shaderSources.size(), shaderAttributes.data(), shaderAttributes.size()},
}};

const mln_plugin_layer_type_v1 layerType = [] {
    mln_plugin_layer_type_v1 value{};
    value.struct_size = sizeof(value);
    value.layer_type = str("rectangle");
    value.backend_mask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN | MLN_PLUGIN_BACKEND_METAL;
    value.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
    value.properties = properties.data();
    value.property_count = properties.size();
    value.source_kind = MLN_PLUGIN_SOURCE_GEOMETRY;
    value.geometry_type_mask = MLN_PLUGIN_GEOMETRY_POINT;
    value.shaders = shaders.data();
    value.shader_count = shaders.size();
    value.create_layout = createLayout;
    value.layout_feature = layoutFeature;
    value.finish_layout = finishLayout;
    value.destroy_layout = destroyLayout;
    value.query_feature = queryFeature;
    return value;
}();

const mln_plugin_descriptor_v1 descriptor = {
    sizeof(mln_plugin_descriptor_v1),
    MLN_PLUGIN_ABI_VERSION_1,
    str("org.maplibre.rectangle-layer"),
    str(MLN_RECTANGLE_PLUGIN_VERSION, sizeof(MLN_RECTANGLE_PLUGIN_VERSION) - 1),
    MLN_PLUGIN_ABI_VERSION_1,
    MLN_PLUGIN_ABI_VERSION_1,
    nullptr,
    0,
    &layerType,
    1,
};

} // namespace

extern "C" mln_plugin_status mln_rectangle_layer_register(mln_plugin_register_function_v1 registerPlugin,
                                                            char* errorMessage,
                                                            size_t errorMessageCapacity) {
    if (!registerPlugin) return MLN_PLUGIN_STATUS_NOT_FOUND;
    return registerPlugin(&descriptor, errorMessage, errorMessageCapacity);
}
