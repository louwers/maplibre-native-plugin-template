#include "gltf_layer.hpp"

#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef MLN_GLTF_PLUGIN_VERSION
#define MLN_GLTF_PLUGIN_VERSION "0.1.0-local"
#endif

namespace {

constexpr uint32_t positionAttribute = 0;
constexpr uint32_t normalAttribute = 1;
constexpr uint32_t colorAttribute = 2;
constexpr uint32_t vertexStream = 0;
constexpr double pi = 3.14159265358979323846;
constexpr double earthCircumference = 40075016.68557849;

constexpr mln_plugin_string str(const char* value, size_t size) { return {value, size}; }
template <size_t N>
constexpr mln_plugin_string str(const char (&value)[N]) { return str(value, N - 1); }

struct Matrix { double value[16]{}; };
struct ModelVertex { float position[3]; float normal[3]; float color[4]; };
struct Model { std::vector<ModelVertex> vertices; std::vector<uint32_t> indices; };
struct Vertex { float position[3]; float normal[3]; float color[4]; };

static_assert(offsetof(Vertex, normal) == 12);
static_assert(offsetof(Vertex, color) == 24);
static_assert(sizeof(Vertex) == 40);

struct Layout {
    std::shared_ptr<const Model> model;
    uint32_t extent = 0;
    double metersToTile = 1.0;
    float altitude = 0.0f;
    float heading = 0.0f;
    float scale = 1.0f;
    float opacity = 1.0f;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<mln_plugin_segment_v1> segments;
    std::array<mln_plugin_vertex_stream_v1, 1> streams{};
    std::array<mln_plugin_attribute_binding_v1, 3> attributes{};
    std::array<mln_plugin_drawable_descriptor_v1, 1> drawables{};
};

struct CachedModel {
    std::mutex mutex;
    std::condition_variable condition;
    bool complete = false;
    std::shared_ptr<const Model> model;
    std::string error;
};

std::mutex modelCacheMutex;
std::unordered_map<std::string, std::shared_ptr<CachedModel>> modelCache;

Matrix identity() {
    Matrix result{};
    result.value[0] = result.value[5] = result.value[10] = result.value[15] = 1.0;
    return result;
}

Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                result.value[column * 4 + row] += a.value[k * 4 + row] * b.value[column * 4 + k];
            }
        }
    }
    return result;
}

Matrix nodeMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        Matrix result{};
        std::copy(node.matrix.begin(), node.matrix.end(), result.value);
        return result;
    }
    Matrix translation = identity();
    if (node.translation.size() == 3) {
        translation.value[12] = node.translation[0];
        translation.value[13] = node.translation[1];
        translation.value[14] = node.translation[2];
    }
    Matrix rotation = identity();
    if (node.rotation.size() == 4) {
        const double x = node.rotation[0], y = node.rotation[1], z = node.rotation[2], w = node.rotation[3];
        rotation.value[0] = 1.0 - 2.0 * (y * y + z * z);
        rotation.value[1] = 2.0 * (x * y + z * w);
        rotation.value[2] = 2.0 * (x * z - y * w);
        rotation.value[4] = 2.0 * (x * y - z * w);
        rotation.value[5] = 1.0 - 2.0 * (x * x + z * z);
        rotation.value[6] = 2.0 * (y * z + x * w);
        rotation.value[8] = 2.0 * (x * z + y * w);
        rotation.value[9] = 2.0 * (y * z - x * w);
        rotation.value[10] = 1.0 - 2.0 * (x * x + y * y);
    }
    Matrix scale = identity();
    if (node.scale.size() == 3) {
        scale.value[0] = node.scale[0]; scale.value[5] = node.scale[1]; scale.value[10] = node.scale[2];
    }
    return multiply(translation, multiply(rotation, scale));
}

double component(const tinygltf::Model& model,
                 const tinygltf::Accessor& accessor,
                 size_t element,
                 size_t componentIndex) {
    if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size()) return 0.0;
    const auto& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= model.buffers.size()) return 0.0;
    const auto& buffer = model.buffers[view.buffer];
    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const int byteStride = accessor.ByteStride(view);
    const size_t stride = byteStride > 0 ? static_cast<size_t>(byteStride)
                                         : componentSize * tinygltf::GetNumComponentsInType(accessor.type);
    const size_t offset = view.byteOffset + accessor.byteOffset + element * stride + componentIndex * componentSize;
    if (offset + componentSize > buffer.data.size()) return 0.0;
    const uint8_t* data = buffer.data.data() + offset;
    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: { float value; std::memcpy(&value, data, sizeof(value)); return value; }
        case TINYGLTF_COMPONENT_TYPE_DOUBLE: { double value; std::memcpy(&value, data, sizeof(value)); return value; }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return *data;
        case TINYGLTF_COMPONENT_TYPE_BYTE: return *reinterpret_cast<const int8_t*>(data);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: { uint16_t value; std::memcpy(&value, data, sizeof(value)); return value; }
        case TINYGLTF_COMPONENT_TYPE_SHORT: { int16_t value; std::memcpy(&value, data, sizeof(value)); return value; }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: { uint32_t value; std::memcpy(&value, data, sizeof(value)); return value; }
        default: return 0.0;
    }
}

void transformPoint(const Matrix& matrix, const double input[3], float output[3]) {
    for (int row = 0; row < 3; ++row) {
        output[row] = static_cast<float>(matrix.value[row] * input[0] + matrix.value[4 + row] * input[1] +
                                         matrix.value[8 + row] * input[2] + matrix.value[12 + row]);
    }
}

void transformNormal(const Matrix& matrix, const double input[3], float output[3]) {
    double length = 0.0;
    for (int row = 0; row < 3; ++row) {
        const double value = matrix.value[row] * input[0] + matrix.value[4 + row] * input[1] +
                             matrix.value[8 + row] * input[2];
        output[row] = static_cast<float>(value);
        length += value * value;
    }
    length = std::sqrt(length);
    if (length > std::numeric_limits<double>::epsilon()) {
        for (size_t i = 0; i < 3; ++i) output[i] = static_cast<float>(output[i] / length);
    }
}

void appendPrimitive(const tinygltf::Model& source,
                     const tinygltf::Primitive& primitive,
                     const Matrix& transform,
                     Model& output) {
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES || primitive.indices < 0) return;
    const auto positionIt = primitive.attributes.find("POSITION");
    if (positionIt == primitive.attributes.end()) return;
    const auto& positions = source.accessors[positionIt->second];
    const auto normalIt = primitive.attributes.find("NORMAL");
    const tinygltf::Accessor* normals = normalIt == primitive.attributes.end() ? nullptr : &source.accessors[normalIt->second];
    double color[4]{0.72, 0.64, 0.48, 1.0};
    if (primitive.material >= 0 && static_cast<size_t>(primitive.material) < source.materials.size()) {
        const auto& factor = source.materials[primitive.material].pbrMetallicRoughness.baseColorFactor;
        if (factor.size() == 4) std::copy(factor.begin(), factor.end(), color);
    }
    const uint32_t vertexOffset = static_cast<uint32_t>(output.vertices.size());
    for (size_t i = 0; i < positions.count; ++i) {
        const double position[3]{component(source, positions, i, 0), component(source, positions, i, 1), component(source, positions, i, 2)};
        const double normal[3]{normals ? component(source, *normals, i, 0) : 0.0,
                               normals ? component(source, *normals, i, 1) : 1.0,
                               normals ? component(source, *normals, i, 2) : 0.0};
        ModelVertex vertex{};
        transformPoint(transform, position, vertex.position);
        transformNormal(transform, normal, vertex.normal);
        for (size_t c = 0; c < 4; ++c) vertex.color[c] = static_cast<float>(color[c]);
        output.vertices.push_back(vertex);
    }
    const auto& indices = source.accessors[primitive.indices];
    for (size_t i = 0; i < indices.count; ++i) {
        const auto index = static_cast<uint32_t>(component(source, indices, i, 0));
        if (index < positions.count) output.indices.push_back(vertexOffset + index);
    }
}

void appendNode(const tinygltf::Model& source, int nodeIndex, const Matrix& parent, Model& output) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= source.nodes.size()) return;
    const auto& node = source.nodes[nodeIndex];
    const auto transform = multiply(parent, nodeMatrix(node));
    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < source.meshes.size()) {
        for (const auto& primitive : source.meshes[node.mesh].primitives) appendPrimitive(source, primitive, transform, output);
    }
    for (const int child : node.children) appendNode(source, child, transform, output);
}

std::shared_ptr<const Model> parseGLB(const uint8_t* bytes, size_t size, std::string& error) {
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader([](tinygltf::Image*, int, std::string*, std::string*, int, int,
                             const unsigned char*, int, void*) { return true; }, nullptr);
    tinygltf::Model source;
    std::string warning;
    if (!loader.LoadBinaryFromMemory(&source, &error, &warning, bytes, static_cast<unsigned int>(size))) {
        if (error.empty()) error = warning.empty() ? "TinyGLTF could not parse the GLB" : warning;
        return {};
    }
    auto result = std::make_shared<Model>();
    const int sceneIndex = source.defaultScene >= 0 ? source.defaultScene : (source.scenes.empty() ? -1 : 0);
    if (sceneIndex >= 0) {
        for (const int node : source.scenes[sceneIndex].nodes) appendNode(source, node, identity(), *result);
    } else {
        for (size_t node = 0; node < source.nodes.size(); ++node) appendNode(source, static_cast<int>(node), identity(), *result);
    }
    if (result->vertices.empty() || result->indices.size() < 3) {
        error = "GLB contains no indexed triangle meshes";
        return {};
    }
    result->indices.resize(result->indices.size() - result->indices.size() % 3);
    return result;
}

void resourceLoaded(void* context, const mln_plugin_resource_response_v1* response) {
    auto* cache = static_cast<CachedModel*>(context);
    if (!cache) return;
    std::shared_ptr<const Model> model;
    std::string error;
    if (!response || !response->data || response->data_size == 0) {
        error = response && response->error_message.data
                    ? std::string(response->error_message.data, response->error_message.size)
                    : "model request returned no data";
    } else {
        model = parseGLB(response->data, response->data_size, error);
    }
    {
        std::lock_guard lock(cache->mutex);
        cache->model = std::move(model);
        cache->error = std::move(error);
        cache->complete = true;
    }
    cache->condition.notify_all();
}

void log(const mln_plugin_host_api_v1* host, int severity, const std::string& message) {
    if (host && host->log) host->log(severity, {message.data(), message.size()});
}

std::shared_ptr<const Model> loadModel(const mln_plugin_host_api_v1* host, const std::string& uri) {
    if (!host || host->struct_size < sizeof(*host) || !host->request_resource || uri.empty()) return {};
    std::shared_ptr<CachedModel> cached;
    bool request = false;
    {
        std::lock_guard lock(modelCacheMutex);
        auto [it, inserted] = modelCache.emplace(uri, nullptr);
        if (inserted) { it->second = std::make_shared<CachedModel>(); request = true; }
        cached = it->second;
    }
    if (request) {
        uint64_t requestID = 0;
        const auto status = host->request_resource(host->context, {uri.data(), uri.size()}, resourceLoaded, cached.get(), &requestID);
        if (status != MLN_PLUGIN_STATUS_OK) {
            {
                std::lock_guard lock(cached->mutex);
                cached->error = "MapLibre file source rejected the model request";
                cached->complete = true;
            }
            cached->condition.notify_all();
        }
    }
    std::unique_lock lock(cached->mutex);
    cached->condition.wait(lock, [&] { return cached->complete; });
    if (!cached->model) log(host, 3, "Failed to load '" + uri + "': " + cached->error);
    return cached->model;
}

const mln_plugin_value* property(const mln_plugin_property_value_v1* properties, size_t count, const char* name) {
    const size_t length = std::strlen(name);
    for (size_t i = 0; i < count; ++i) {
        const auto& candidate = properties[i];
        if (candidate.name.data && candidate.name.size == length && std::memcmp(candidate.name.data, name, length) == 0) {
            return &candidate.value;
        }
    }
    return nullptr;
}

float number(const mln_plugin_property_value_v1* properties, size_t count, const char* name, float fallback) {
    const auto* value = property(properties, count, name);
    return value && value->type == MLN_PLUGIN_VALUE_FLOAT && std::isfinite(value->data.float_value)
               ? value->data.float_value : fallback;
}

std::string string(const mln_plugin_property_value_v1* properties, size_t count, const char* name) {
    const auto* value = property(properties, count, name);
    return value && value->type == MLN_PLUGIN_VALUE_STRING && value->data.string_value.data
               ? std::string(value->data.string_value.data, value->data.string_value.size) : std::string{};
}

void startSegment(Layout& layout) {
    mln_plugin_segment_v1 segment{};
    segment.struct_size = sizeof(segment);
    segment.vertex_offset = static_cast<uint32_t>(layout.vertices.size());
    segment.index_offset = static_cast<uint32_t>(layout.indices.size());
    layout.segments.push_back(segment);
}

mln_plugin_status createLayout(const mln_plugin_layout_context_v1* context, void** output) {
    if (!context || context->struct_size < sizeof(*context) || !context->host || !output) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    auto layout = std::make_unique<Layout>();
    const auto uri = string(context->properties, context->property_count, "model-uri");
    layout->model = loadModel(context->host, uri);
    if (!layout->model) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    layout->altitude = number(context->properties, context->property_count, "model-altitude", 0.0f);
    layout->heading = number(context->properties, context->property_count, "model-heading", 0.0f) * static_cast<float>(pi / 180.0);
    layout->scale = number(context->properties, context->property_count, "model-scale", 1.0f);
    layout->opacity = std::clamp(number(context->properties, context->property_count, "model-opacity", 1.0f), 0.0f, 1.0f);
    layout->extent = context->extent;
    const double centerTileY = static_cast<double>(context->canonical_y) + 0.5;
    const double mercatorY = centerTileY / std::exp2(context->canonical_z);
    const double latitude = std::atan(std::sinh(pi * (1.0 - 2.0 * mercatorY)));
    layout->metersToTile = static_cast<double>(context->extent) * std::exp2(context->canonical_z) /
                           (earthCircumference * std::max(0.01, std::cos(latitude)));
    startSegment(*layout);
    *output = layout.release();
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status layoutFeature(void* instance, const mln_plugin_feature_v1* feature) {
    if (!instance || !feature || feature->struct_size < sizeof(*feature) ||
        feature->geometry_type != MLN_PLUGIN_GEOMETRY_POINT || !feature->points) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    auto& layout = *static_cast<Layout*>(instance);
    if (!layout.model) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    const float cosine = std::cos(layout.heading), sine = std::sin(layout.heading);
    const float horizontalScale = static_cast<float>(layout.metersToTile) * layout.scale;
    for (size_t pointIndex = 0; pointIndex < feature->point_count; ++pointIndex) {
        const auto anchor = feature->points[pointIndex];
        // GeoJSON tile buffers can repeat the same point in adjacent tiles.
        // Emit it only from the canonical tile that owns the anchor.
        if (anchor.x < 0 || anchor.y < 0 || anchor.x >= static_cast<int32_t>(layout.extent) ||
            anchor.y >= static_cast<int32_t>(layout.extent)) {
            continue;
        }
        std::unordered_map<uint32_t, uint16_t> localIndices;
        for (size_t triangle = 0; triangle + 2 < layout.model->indices.size(); triangle += 3) {
            size_t missing = 0;
            for (size_t corner = 0; corner < 3; ++corner) missing += localIndices.count(layout.model->indices[triangle + corner]) == 0;
            if (layout.segments.back().vertex_length + missing > std::numeric_limits<uint16_t>::max()) {
                startSegment(layout);
                localIndices.clear();
            }
            auto& segment = layout.segments.back();
            for (size_t corner = 0; corner < 3; ++corner) {
                const uint32_t modelIndex = layout.model->indices[triangle + corner];
                if (modelIndex >= layout.model->vertices.size()) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
                auto found = localIndices.find(modelIndex);
                if (found == localIndices.end()) {
                    const auto localIndex = static_cast<uint16_t>(segment.vertex_length);
                    const auto& source = layout.model->vertices[modelIndex];
                    Vertex vertex{};
                    vertex.position[0] = anchor.x + (cosine * source.position[0] + sine * source.position[2]) * horizontalScale;
                    vertex.position[1] = anchor.y + (sine * source.position[0] - cosine * source.position[2]) * horizontalScale;
                    vertex.position[2] = layout.altitude + source.position[1] * layout.scale;
                    vertex.normal[0] = cosine * source.normal[0] + sine * source.normal[2];
                    vertex.normal[1] = sine * source.normal[0] - cosine * source.normal[2];
                    vertex.normal[2] = source.normal[1];
                    for (size_t c = 0; c < 3; ++c) vertex.color[c] = source.color[c] * layout.opacity;
                    vertex.color[3] = source.color[3] * layout.opacity;
                    layout.vertices.push_back(vertex);
                    found = localIndices.emplace(modelIndex, localIndex).first;
                    ++segment.vertex_length;
                }
                layout.indices.push_back(found->second);
                ++segment.index_length;
            }
            segment.feature_index = feature->feature_index;
        }
    }
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status finishLayout(void* instance, mln_plugin_bucket_v1* output) {
    if (!instance || !output || output->struct_size < sizeof(*output)) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    auto& layout = *static_cast<Layout*>(instance);
    layout.segments.erase(std::remove_if(layout.segments.begin(), layout.segments.end(),
                                         [](const auto& segment) { return segment.index_length == 0; }), layout.segments.end());
    layout.streams[0] = {sizeof(mln_plugin_vertex_stream_v1), vertexStream,
                         reinterpret_cast<const uint8_t*>(layout.vertices.data()), layout.vertices.size() * sizeof(Vertex),
                         static_cast<uint32_t>(layout.vertices.size()), sizeof(Vertex)};
    layout.attributes = {{
        {sizeof(mln_plugin_attribute_binding_v1), positionAttribute, vertexStream, offsetof(Vertex, position), MLN_PLUGIN_VERTEX_FLOAT_X3},
        {sizeof(mln_plugin_attribute_binding_v1), normalAttribute, vertexStream, offsetof(Vertex, normal), MLN_PLUGIN_VERTEX_FLOAT_X3},
        {sizeof(mln_plugin_attribute_binding_v1), colorAttribute, vertexStream, offsetof(Vertex, color), MLN_PLUGIN_VERTEX_FLOAT_X4},
    }};
    auto& drawable = layout.drawables[0];
    drawable.struct_size = sizeof(drawable);
    drawable.drawable_key = 1;
    drawable.shader_id = str("gltf");
    drawable.draw_mode = MLN_PLUGIN_DRAW_MODE_TRIANGLES;
    drawable.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
    drawable.depth_mode = MLN_PLUGIN_DEPTH_READ_WRITE;
    drawable.blend_mode = MLN_PLUGIN_BLEND_PREMULTIPLIED_ALPHA;
    drawable.enable_stencil = 0;
    // GLB assets may mix material winding/double-sided declarations. Keep the
    // initial host-owned renderer faithful to the previous plugin by drawing
    // both faces until material.cullMode is represented in bucket metadata.
    drawable.enable_cull_face = 0;
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
    output->query_radius = 0.0f;
    return MLN_PLUGIN_STATUS_OK;
}

void destroyLayout(void* instance) { delete static_cast<Layout*>(instance); }

constexpr char glVertex[] = R"SHADER(
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec4 a_color;
layout (std140) uniform PluginDrawableUBO { mat4 u_matrix; vec2 u_extrude_scale; vec2 u_pad; };
out vec3 v_normal;
out vec4 v_color;
void main() { gl_Position = u_matrix * vec4(a_position, 1.0); v_normal = a_normal; v_color = a_color; }
)SHADER";

constexpr char glFragment[] = R"SHADER(
in vec3 v_normal;
in vec4 v_color;
void main() {
    float light = 0.32 + 0.68 * max(dot(normalize(v_normal), normalize(vec3(0.35, -0.45, 0.82))), 0.0);
    fragColor = vec4(v_color.rgb * light, v_color.a);
}
)SHADER";

constexpr char vkVertex[] = R"SHADER(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
layout(std140, set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform PluginDrawableUBO {
    mat4 matrix;
    vec2 extrude_scale;
    vec2 pad;
} drawable;
layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec4 v_color;
void main() {
    gl_Position = drawable.matrix * vec4(a_position, 1.0);
    applySurfaceTransform(); v_normal = a_normal; v_color = a_color;
}
)SHADER";

constexpr char vkFragment[] = R"SHADER(
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec4 v_color;
layout(location = 0) out vec4 fragColor;
void main() {
    float light = 0.32 + 0.68 * max(dot(normalize(v_normal), normalize(vec3(0.35, -0.45, 0.82))), 0.0);
    fragColor = vec4(v_color.rgb * light, v_color.a);
}
)SHADER";

constexpr char metalSource[] = R"SHADER(
struct alignas(16) PluginDrawableUBO { float4x4 matrix; float2 extrude_scale; float2 pad; };
struct GltfVertex { float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; float4 color [[attribute(2)]]; };
struct GltfFragment { float4 position [[position, invariant]]; float3 normal; float4 color; };
GltfFragment vertex gltfVertex(thread const GltfVertex vertx [[stage_in]],
    device const PluginDrawableUBO& drawable [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {
    return {
        .position = drawable.matrix * float4(vertx.position, 1.0),
        .normal = vertx.normal,
        .color = vertx.color
    };
}
half4 fragment gltfFragment(GltfFragment in [[stage_in]]) {
    float light = 0.32 + 0.68 * max(dot(normalize(in.normal), normalize(float3(0.35, -0.45, 0.82))), 0.0);
    return half4(float4(in.color.rgb * light, in.color.a));
}
)SHADER";

struct alignas(16) DrawableUBO {
    float matrix[16];
    float extrudeScale[2];
    float padding[2];
};
static_assert(sizeof(DrawableUBO) == 80);

mln_plugin_status updateUniform(const mln_plugin_uniform_context_v1* context,
                                uint32_t uniformID,
                                uint8_t* output,
                                size_t outputSize) {
    if (!context || context->struct_size < sizeof(*context) || !output || uniformID != 0 ||
        outputSize != sizeof(DrawableUBO)) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    DrawableUBO value{};
    std::copy_n(context->tile_matrix, 16, value.matrix);
    value.extrudeScale[0] = context->pixels_to_gl_units[0];
    value.extrudeScale[1] = context->pixels_to_gl_units[1];
    std::memcpy(output, &value, sizeof(value));
    return MLN_PLUGIN_STATUS_OK;
}

constexpr mln_plugin_value makeFloat(float value) {
    mln_plugin_value result{}; result.struct_size = sizeof(result); result.type = MLN_PLUGIN_VALUE_FLOAT;
    result.data.float_value = value; return result;
}
constexpr mln_plugin_value makeString() {
    mln_plugin_value result{}; result.struct_size = sizeof(result); result.type = MLN_PLUGIN_VALUE_STRING; return result;
}

const std::array<mln_plugin_property_descriptor_v1, 5> properties = {{
    {sizeof(mln_plugin_property_descriptor_v1), str("model-uri"), MLN_PLUGIN_VALUE_STRING, MLN_PLUGIN_PROPERTY_LAYOUT, makeString(), 0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("model-altitude"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_LAYOUT, makeFloat(0), 0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("model-heading"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_LAYOUT, makeFloat(0), 0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("model-scale"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_LAYOUT, makeFloat(1), 0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("model-opacity"), MLN_PLUGIN_VALUE_FLOAT, MLN_PLUGIN_PROPERTY_PAINT, makeFloat(1), 0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0},
}};
const std::array<mln_plugin_shader_attribute_v1, 3> shaderAttributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), positionAttribute, 0, str("a_position"), MLN_PLUGIN_VERTEX_FLOAT_X3},
    {sizeof(mln_plugin_shader_attribute_v1), normalAttribute, 1, str("a_normal"), MLN_PLUGIN_VERTEX_FLOAT_X3},
    {sizeof(mln_plugin_shader_attribute_v1), colorAttribute, 2, str("a_color"), MLN_PLUGIN_VERTEX_FLOAT_X4},
}};
const std::array<mln_plugin_shader_source_v1, 3> shaderSources = {{
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_OPENGL, str(glVertex), str(glFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_VULKAN, str(vkVertex), str(vkFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_METAL, str(metalSource), {}, str("gltfVertex"), str("gltfFragment")},
}};
const std::array<mln_plugin_uniform_block_descriptor_v1, 1> shaderUniforms = {{
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     0,
     str("PluginDrawableUBO"),
     sizeof(DrawableUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX,
     MLN_PLUGIN_UNIFORM_SCOPE_DRAWABLE},
}};
const std::array<mln_plugin_shader_descriptor_v1, 1> shaders = {{
    {sizeof(mln_plugin_shader_descriptor_v1),
     str("gltf"),
     shaderSources.data(),
     shaderSources.size(),
     shaderAttributes.data(),
     shaderAttributes.size(),
     shaderUniforms.data(),
     shaderUniforms.size(),
     nullptr,
     0},
}};

const mln_plugin_layer_type_v1 layerType = [] {
    mln_plugin_layer_type_v1 value{};
    value.struct_size = sizeof(value);
    value.layer_type = str("gltf");
    value.backend_mask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN | MLN_PLUGIN_BACKEND_METAL;
    value.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
    value.requires_3d = 1;
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
    value.update_uniform_block = updateUniform;
    return value;
}();

const mln_plugin_descriptor_v1 descriptor = {
    sizeof(mln_plugin_descriptor_v1), MLN_PLUGIN_ABI_VERSION_1,
    str("org.maplibre.gltf-layer"), str(MLN_GLTF_PLUGIN_VERSION, sizeof(MLN_GLTF_PLUGIN_VERSION) - 1),
    MLN_PLUGIN_ABI_VERSION_1, MLN_PLUGIN_ABI_VERSION_1, nullptr, 0, &layerType, 1,
};

} // namespace

extern "C" mln_plugin_status mln_gltf_layer_register(mln_plugin_register_function_v1 registerPlugin,
                                                      char* errorMessage,
                                                      size_t errorMessageCapacity) {
    if (!registerPlugin) return MLN_PLUGIN_STATUS_NOT_FOUND;
    return registerPlugin(&descriptor, errorMessage, errorMessageCapacity);
}
