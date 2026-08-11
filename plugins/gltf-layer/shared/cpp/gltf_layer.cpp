#include "gltf_layer.hpp"

#include <tiny_gltf.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

#define MLN_GLTF_STRINGIFY_INNER(value) #value
#define MLN_GLTF_STRINGIFY(value) MLN_GLTF_STRINGIFY_INNER(value)

constexpr char pluginID[] = "org.maplibre.gltf-layer";
constexpr char pluginVersion[] = MLN_GLTF_STRINGIFY(MLN_GLTF_PLUGIN_VERSION);
constexpr char layerTypeName[] = "gltf";
constexpr char modelURIName[] = "model-uri";
constexpr char modelPositionName[] = "model-position";
constexpr char modelAltitudeName[] = "model-altitude";
constexpr char modelHeadingName[] = "model-heading";
constexpr char modelScaleName[] = "model-scale";
constexpr char modelOpacityName[] = "model-opacity";

std::atomic<uint64_t> prepareCallbackCount{0};
std::atomic<uint64_t> loadCallbackCount{0};
std::atomic<uint64_t> renderCallbackCount{0};
std::atomic<uint64_t> loadedVertexCount{0};

constexpr mln_plugin_string pluginString(const char* value, size_t size) {
    return {value, size - 1};
}

struct Matrix {
    double value[16]{};
};

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
        const double x = node.rotation[0];
        const double y = node.rotation[1];
        const double z = node.rotation[2];
        const double w = node.rotation[3];
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
        scale.value[0] = node.scale[0];
        scale.value[5] = node.scale[1];
        scale.value[10] = node.scale[2];
    }
    return multiply(translation, multiply(rotation, scale));
}

double component(const tinygltf::Model& model,
                 const tinygltf::Accessor& accessor,
                 size_t element,
                 size_t componentIndex) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const int byteStride = accessor.ByteStride(view);
    const size_t stride = byteStride > 0 ? static_cast<size_t>(byteStride)
                                         : componentSize * tinygltf::GetNumComponentsInType(accessor.type);
    const uint8_t* data = buffer.data.data() + view.byteOffset + accessor.byteOffset + element * stride +
                          componentIndex * componentSize;
    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            float value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        case TINYGLTF_COMPONENT_TYPE_DOUBLE: {
            double value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *data;
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            return *reinterpret_cast<const int8_t*>(data);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            int16_t value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            uint32_t value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        default:
            return 0.0;
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

bool appendPrimitive(const tinygltf::Model& source,
                     const tinygltf::Primitive& primitive,
                     const Matrix& transform,
                     GltfModelData& output) {
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES || primitive.indices < 0) return true;
    const auto positionIt = primitive.attributes.find("POSITION");
    if (positionIt == primitive.attributes.end()) return true;
    const auto& positions = source.accessors[positionIt->second];
    const auto normalIt = primitive.attributes.find("NORMAL");
    const tinygltf::Accessor* normals =
        normalIt == primitive.attributes.end() ? nullptr : &source.accessors[normalIt->second];

    double color[4]{0.72, 0.64, 0.48, 1.0};
    if (primitive.material >= 0 && static_cast<size_t>(primitive.material) < source.materials.size()) {
        const auto& factor = source.materials[primitive.material].pbrMetallicRoughness.baseColorFactor;
        if (factor.size() == 4) std::copy(factor.begin(), factor.end(), color);
    }

    const uint32_t vertexOffset = static_cast<uint32_t>(output.vertices.size());
    output.vertices.reserve(output.vertices.size() + positions.count);
    for (size_t i = 0; i < positions.count; ++i) {
        const double position[3]{component(source, positions, i, 0),
                                 component(source, positions, i, 1),
                                 component(source, positions, i, 2)};
        const double normal[3]{normals ? component(source, *normals, i, 0) : 0.0,
                               normals ? component(source, *normals, i, 1) : 1.0,
                               normals ? component(source, *normals, i, 2) : 0.0};
        GltfVertex vertex{};
        transformPoint(transform, position, vertex.position);
        transformNormal(transform, normal, vertex.normal);
        for (size_t c = 0; c < 4; ++c) vertex.color[c] = static_cast<float>(color[c]);
        output.vertices.push_back(vertex);
    }

    const auto& indices = source.accessors[primitive.indices];
    output.indices.reserve(output.indices.size() + indices.count);
    for (size_t i = 0; i < indices.count; ++i) {
        output.indices.push_back(vertexOffset + static_cast<uint32_t>(component(source, indices, i, 0)));
    }
    return true;
}

void appendNode(const tinygltf::Model& source, int nodeIndex, const Matrix& parent, GltfModelData& output) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= source.nodes.size()) return;
    const auto& node = source.nodes[nodeIndex];
    const auto transform = multiply(parent, nodeMatrix(node));
    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < source.meshes.size()) {
        for (const auto& primitive : source.meshes[node.mesh].primitives) {
            appendPrimitive(source, primitive, transform, output);
        }
    }
    for (const int child : node.children) appendNode(source, child, transform, output);
}

std::shared_ptr<GltfModelData> parseGLB(const uint8_t* bytes, size_t size, std::string& error) {
    tinygltf::TinyGLTF loader;
    // Textures are not uploaded in the initial renderer, so retain their glTF
    // metadata without paying to decode dozens of embedded PNG/JPEG images.
    loader.SetImageLoader([](tinygltf::Image*,
                             int,
                             std::string*,
                             std::string*,
                             int,
                             int,
                             const unsigned char*,
                             int,
                             void*) { return true; },
                         nullptr);
    tinygltf::Model source;
    std::string warning;
    if (!loader.LoadBinaryFromMemory(&source, &error, &warning, bytes, static_cast<unsigned int>(size))) {
        if (error.empty()) error = warning.empty() ? "TinyGLTF could not parse the GLB" : warning;
        return {};
    }
    auto result = std::make_shared<GltfModelData>();
    const int sceneIndex = source.defaultScene >= 0 ? source.defaultScene : (source.scenes.empty() ? -1 : 0);
    if (sceneIndex >= 0) {
        for (const int node : source.scenes[sceneIndex].nodes) appendNode(source, node, identity(), *result);
    } else {
        for (size_t node = 0; node < source.nodes.size(); ++node) {
            appendNode(source, static_cast<int>(node), identity(), *result);
        }
    }
    if (result->vertices.empty() || result->indices.empty()) {
        error = "GLB contains no triangle meshes";
        return {};
    }
    return result;
}

void resourceLoaded(void* context, const mln_plugin_resource_response_v1* response) {
    ++loadCallbackCount;
    auto* instance = static_cast<GltfLayerInstance*>(context);
    if (!instance || !response || response->request_id != instance->requestID) return;
    if (!response->data || response->data_size == 0) {
        const std::string error = response->error_message.data
                                      ? std::string(response->error_message.data, response->error_message.size)
                                      : "model request returned no data";
        gltfLog(instance, 3, "Failed to load '" + instance->requestedURI + "': " + error);
        return;
    }
    std::string error;
    auto model = parseGLB(response->data, response->data_size, error);
    if (!model) {
        gltfLog(instance, 3, "Failed to parse '" + instance->requestedURI + "': " + error);
        return;
    }
    instance->model = std::move(model);
    loadedVertexCount = instance->model->vertices.size();
    ++instance->modelGeneration;
    gltfLog(instance,
            1,
            "Loaded GLB with " + std::to_string(instance->model->vertices.size()) + " vertices and " +
                std::to_string(instance->model->indices.size() / 3) + " triangles");
}

mln_plugin_status createInstance(const mln_plugin_host_api_v1* host, mln_plugin_string, void** output) {
    if (!host || !output || host->abi_version != MLN_PLUGIN_ABI_VERSION_1 || !host->request_resource) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto* instance = new GltfLayerInstance();
    instance->host = host;
    *output = instance;
    return MLN_PLUGIN_STATUS_OK;
}

void destroyInstance(void* opaque) {
    auto* instance = static_cast<GltfLayerInstance*>(opaque);
    if (!instance) return;
    if (instance->requestID && instance->host->cancel_resource_request) {
        instance->host->cancel_resource_request(instance->host->context, instance->requestID);
    }
#if defined(MLN_GLTF_ANDROID)
    gltfDestroyOpenGL(instance);
    gltfDestroyVulkan(instance);
#elif defined(MLN_GLTF_IOS)
    gltfDestroyMetal(instance);
#endif
    delete instance;
}

mln_plugin_status prepareFrame(void* opaque, const mln_plugin_frame_context_v1* frame) {
    ++prepareCallbackCount;
    auto* instance = static_cast<GltfLayerInstance*>(opaque);
    if (!instance || !frame) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    const auto* uriProperty = gltfProperty(frame, modelURIName);
    const std::string uri = uriProperty && uriProperty->value.type == MLN_PLUGIN_VALUE_STRING &&
                                    uriProperty->value.data.string_value.data
                                ? std::string(uriProperty->value.data.string_value.data,
                                              uriProperty->value.data.string_value.size)
                                : std::string{};
    if (uri == instance->requestedURI) return MLN_PLUGIN_STATUS_OK;
    if (instance->requestID && instance->host->cancel_resource_request) {
        instance->host->cancel_resource_request(instance->host->context, instance->requestID);
    }
    instance->requestID = 0;
    instance->requestedURI = uri;
    instance->model.reset();
    ++instance->modelGeneration;
    if (uri.empty()) return MLN_PLUGIN_STATUS_OK;
    return instance->host->request_resource(instance->host->context,
                                            {uri.data(), uri.size()},
                                            resourceLoaded,
                                            instance,
                                            &instance->requestID);
}

mln_plugin_status renderLayer(void* opaque, const mln_plugin_frame_context_v1* frame) {
    ++renderCallbackCount;
    auto* instance = static_cast<GltfLayerInstance*>(opaque);
    if (!instance || !frame || !frame->backend || frame->stage == MLN_PLUGIN_RENDER_STAGE_PREPARE) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    if (!instance->model) return MLN_PLUGIN_STATUS_OK;
    switch (frame->backend->backend) {
#if defined(MLN_GLTF_ANDROID)
        case MLN_PLUGIN_BACKEND_OPENGL:
            return gltfRenderOpenGL(instance, frame);
        case MLN_PLUGIN_BACKEND_VULKAN:
            return gltfRenderVulkan(instance, frame);
#elif defined(MLN_GLTF_IOS)
        case MLN_PLUGIN_BACKEND_METAL:
            return gltfRenderMetal(instance, frame);
#endif
        default:
            return MLN_PLUGIN_STATUS_OK;
    }
}

void contextLost(void* opaque) {
    auto* instance = static_cast<GltfLayerInstance*>(opaque);
    if (!instance) return;
#if defined(MLN_GLTF_ANDROID)
    gltfDestroyOpenGL(instance);
    gltfDestroyVulkan(instance);
#elif defined(MLN_GLTF_IOS)
    gltfDestroyMetal(instance);
#endif
}

const mln_plugin_property_descriptor_v1 properties[]{
    {sizeof(mln_plugin_property_descriptor_v1),
     pluginString(modelURIName, sizeof(modelURIName)),
     MLN_PLUGIN_VALUE_STRING,
     MLN_PLUGIN_PROPERTY_LAYOUT,
     {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_STRING, {.string_value = {"", 0}}}},
    {sizeof(mln_plugin_property_descriptor_v1),
     pluginString(modelPositionName, sizeof(modelPositionName)),
     MLN_PLUGIN_VALUE_FLOAT2,
     MLN_PLUGIN_PROPERTY_LAYOUT,
     {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_FLOAT2, {.float2_value = {0.0f, 0.0f}}}},
    {sizeof(mln_plugin_property_descriptor_v1),
     pluginString(modelAltitudeName, sizeof(modelAltitudeName)),
     MLN_PLUGIN_VALUE_FLOAT,
     MLN_PLUGIN_PROPERTY_LAYOUT,
     {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_FLOAT, {.float_value = 0.0f}}},
    {sizeof(mln_plugin_property_descriptor_v1),
     pluginString(modelHeadingName, sizeof(modelHeadingName)),
     MLN_PLUGIN_VALUE_FLOAT,
     MLN_PLUGIN_PROPERTY_LAYOUT,
     {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_FLOAT, {.float_value = 0.0f}}},
    {sizeof(mln_plugin_property_descriptor_v1),
     pluginString(modelScaleName, sizeof(modelScaleName)),
     MLN_PLUGIN_VALUE_FLOAT,
     MLN_PLUGIN_PROPERTY_LAYOUT,
     {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_FLOAT, {.float_value = 1.0f}}},
    {sizeof(mln_plugin_property_descriptor_v1),
     pluginString(modelOpacityName, sizeof(modelOpacityName)),
     MLN_PLUGIN_VALUE_FLOAT,
     MLN_PLUGIN_PROPERTY_PAINT,
     {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_FLOAT, {.float_value = 1.0f}}},
};

#if defined(MLN_GLTF_ANDROID)
constexpr uint32_t backendMask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN;
#elif defined(MLN_GLTF_IOS)
constexpr uint32_t backendMask = MLN_PLUGIN_BACKEND_METAL;
#else
#error "Define MLN_GLTF_ANDROID or MLN_GLTF_IOS"
#endif

const mln_plugin_layer_type_v1 layerType{sizeof(mln_plugin_layer_type_v1),
                                         pluginString(layerTypeName, sizeof(layerTypeName)),
                                         backendMask,
                                         MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT,
                                         1,
                                         properties,
                                         sizeof(properties) / sizeof(properties[0]),
                                         createInstance,
                                         destroyInstance,
                                         prepareFrame,
                                         renderLayer,
                                         contextLost};

const mln_plugin_descriptor_v1 descriptor{sizeof(mln_plugin_descriptor_v1),
                                          MLN_PLUGIN_ABI_VERSION_1,
                                          pluginString(pluginID, sizeof(pluginID)),
                                          pluginString(pluginVersion, sizeof(pluginVersion)),
                                          MLN_PLUGIN_ABI_VERSION_1,
                                          MLN_PLUGIN_ABI_VERSION_1,
                                          nullptr,
                                          0,
                                          &layerType,
                                          1};

double floatProperty(const mln_plugin_frame_context_v1* frame, const char* name, double fallback) {
    const auto* property = gltfProperty(frame, name);
    return property && property->value.type == MLN_PLUGIN_VALUE_FLOAT ? property->value.data.float_value : fallback;
}

} // namespace

const mln_plugin_property_value_v1* gltfProperty(const mln_plugin_frame_context_v1* frame, const char* name) {
    if (!frame || !name) return nullptr;
    const size_t length = std::strlen(name);
    for (size_t i = 0; i < frame->property_count; ++i) {
        const auto& property = frame->properties[i];
        if (property.name.data && property.name.size == length && std::memcmp(property.name.data, name, length) == 0) {
            return &property;
        }
    }
    return nullptr;
}

bool gltfModelMatrix(const mln_plugin_frame_context_v1* frame, float output[16], float& opacity) {
    if (!frame || !frame->camera || !output) return false;
    const auto* position = gltfProperty(frame, modelPositionName);
    if (!position || position->value.type != MLN_PLUGIN_VALUE_FLOAT2) return false;
    const double longitude = position->value.data.float2_value.x;
    const double latitude = std::clamp<double>(position->value.data.float2_value.y, -85.05112878, 85.05112878);
    const double radians = latitude * 3.14159265358979323846 / 180.0;
    const double mercatorX = (longitude + 180.0) / 360.0;
    const double mercatorY = (1.0 - std::asinh(std::tan(radians)) / 3.14159265358979323846) * 0.5;
    // TransformState's projection matrix consumes x/y in world pixels and z
    // in meters. Horizontal model dimensions therefore need a pixels-per-
    // meter scale, while vertical dimensions and altitude remain in meters.
    const double worldSize = 512.0 * std::exp2(frame->camera->zoom);
    const double pixelsPerMeter = worldSize / (40075016.68557849 * std::cos(radians));
    const double modelScale = floatProperty(frame, modelScaleName, 1.0);
    const double horizontalScale = modelScale * pixelsPerMeter;
    const double verticalScale = modelScale;
    const double altitude = floatProperty(frame, modelAltitudeName, 0.0);
    const double heading = floatProperty(frame, modelHeadingName, 0.0) * 3.14159265358979323846 / 180.0;
    const double cosine = std::cos(heading);
    const double sine = std::sin(heading);

    Matrix model = identity();
    model.value[0] = cosine * horizontalScale;
    model.value[1] = sine * horizontalScale;
    model.value[4] = 0.0;
    model.value[5] = 0.0;
    model.value[6] = verticalScale;
    model.value[8] = sine * horizontalScale;
    model.value[9] = -cosine * horizontalScale;
    model.value[10] = 0.0;
    model.value[12] = mercatorX * worldSize;
    model.value[13] = mercatorY * worldSize;
    model.value[14] = altitude;

    Matrix projection{};
    std::copy(frame->camera->near_clipped_projection_matrix,
              frame->camera->near_clipped_projection_matrix + 16,
              projection.value);
    auto matrix = multiply(projection, model);
    if (frame->backend && frame->backend->backend == MLN_PLUGIN_BACKEND_VULKAN &&
        frame->backend->screen_pre_rotation_radians_clockwise != 0.0f) {
        const double angle = frame->backend->screen_pre_rotation_radians_clockwise;
        Matrix rotation = identity();
        rotation.value[0] = std::cos(angle);
        rotation.value[1] = -std::sin(angle);
        rotation.value[4] = std::sin(angle);
        rotation.value[5] = std::cos(angle);
        matrix = multiply(rotation, matrix);
    }
    for (size_t i = 0; i < 16; ++i) output[i] = static_cast<float>(matrix.value[i]);
    opacity = static_cast<float>(std::clamp(floatProperty(frame, modelOpacityName, 1.0), 0.0, 1.0));
    return true;
}

void gltfLog(GltfLayerInstance* instance, int severity, const std::string& message) {
    if (instance && instance->host && instance->host->log) {
        instance->host->log(severity, {message.data(), message.size()});
    }
}

extern "C" mln_plugin_status mln_gltf_layer_register(mln_plugin_register_function_v1 registerFunction,
                                                      char* errorMessage,
                                                      size_t errorMessageCapacity) {
    if (!registerFunction) return MLN_PLUGIN_STATUS_NOT_FOUND;
    return registerFunction(&descriptor, errorMessage, errorMessageCapacity);
}

extern "C" uint64_t mln_gltf_layer_prepare_callback_count(void) {
    return prepareCallbackCount.load();
}

extern "C" uint64_t mln_gltf_layer_load_callback_count(void) {
    return loadCallbackCount.load();
}

extern "C" uint64_t mln_gltf_layer_render_callback_count(void) {
    return renderCallbackCount.load();
}

extern "C" uint64_t mln_gltf_layer_loaded_vertex_count(void) {
    return loadedVertexCount.load();
}
