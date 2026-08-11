#include "shadow_renderer.hpp"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr char pluginID[] = "org.maplibre.fill-extrusion-shadows";
constexpr char pluginVersion[] = MLN_SHADOW_PLUGIN_VERSION;
constexpr char layerType[] = "fill-extrusion";
constexpr char propertyName[] = "fill-extrusion-shadow";
uint64_t renderCallbackCount = 0;

constexpr mln_plugin_string pluginString(const char* value, size_t size) {
    return {value, size - 1};
}

mln_plugin_status createInstance(const mln_plugin_host_api_v1* host, mln_plugin_string, void** instance) {
    if (!host || !instance || host->abi_version != MLN_PLUGIN_ABI_VERSION_1) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto* created = static_cast<ShadowInstance*>(calloc(1, sizeof(ShadowInstance)));
    if (!created) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    created->host = host;
    *instance = created;
    return MLN_PLUGIN_STATUS_OK;
}

void destroyInstance(void* opaque) {
    auto* instance = static_cast<ShadowInstance*>(opaque);
    if (!instance) return;
#if defined(MLN_SHADOW_ANDROID)
    shadowOpenGLDestroy(instance);
    shadowVulkanDestroy(instance);
#elif defined(MLN_SHADOW_IOS)
    shadowMetalDestroy(instance);
#endif
    free(instance);
}

mln_plugin_status prepareFrame(void* opaque, const mln_plugin_frame_context_v1* frame) {
    auto* instance = static_cast<ShadowInstance*>(opaque);
    if (!instance || !frame || !frame->backend) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    instance->lastBackend = frame->backend->backend;
    if (!shadowEnabled(frame)) return MLN_PLUGIN_STATUS_OK;
    switch (frame->backend->backend) {
#if defined(MLN_SHADOW_ANDROID)
        case MLN_PLUGIN_BACKEND_OPENGL:
            return shadowOpenGLPrepare(instance, frame);
        case MLN_PLUGIN_BACKEND_VULKAN:
            return shadowVulkanPrepare(instance, frame);
#elif defined(MLN_SHADOW_IOS)
        case MLN_PLUGIN_BACKEND_METAL:
            return shadowMetalPrepare(instance, frame);
#endif
        default:
            return MLN_PLUGIN_STATUS_OK;
    }
}

mln_plugin_status renderBeforeLayer(void* opaque, const mln_plugin_frame_context_v1* frame) {
    auto* instance = static_cast<ShadowInstance*>(opaque);
    if (!instance || !frame || !frame->backend) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    if (!shadowEnabled(frame) || frame->stage != MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT) {
        return MLN_PLUGIN_STATUS_OK;
    }
    instance->lastBackend = frame->backend->backend;
    auto status = MLN_PLUGIN_STATUS_OK;
    switch (frame->backend->backend) {
#if defined(MLN_SHADOW_ANDROID)
        case MLN_PLUGIN_BACKEND_OPENGL:
            status = shadowOpenGLRender(instance, frame);
            break;
        case MLN_PLUGIN_BACKEND_VULKAN:
            status = shadowVulkanRender(instance, frame);
            break;
#elif defined(MLN_SHADOW_IOS)
        case MLN_PLUGIN_BACKEND_METAL:
            status = shadowMetalRender(instance, frame);
            break;
#endif
        default:
            break;
    }
    if (status == MLN_PLUGIN_STATUS_OK) {
        __atomic_fetch_add(&renderCallbackCount, uint64_t{1}, __ATOMIC_RELAXED);
    }
    return status;
}

void contextLost(void* opaque) {
    auto* instance = static_cast<ShadowInstance*>(opaque);
    if (!instance) return;
#if defined(MLN_SHADOW_ANDROID)
    shadowOpenGLContextLost(instance);
    shadowVulkanContextLost(instance);
#elif defined(MLN_SHADOW_IOS)
    shadowMetalContextLost(instance);
#endif
}

const mln_plugin_property_descriptor_v1 propertyDescriptor{
    sizeof(mln_plugin_property_descriptor_v1),
    pluginString(propertyName, sizeof(propertyName)),
    MLN_PLUGIN_VALUE_BOOLEAN,
    MLN_PLUGIN_PROPERTY_PAINT,
    {sizeof(mln_plugin_value), MLN_PLUGIN_VALUE_BOOLEAN, {.boolean_value = 0}}};

#if defined(MLN_SHADOW_ANDROID)
constexpr uint32_t backendMask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN;
#elif defined(MLN_SHADOW_IOS)
constexpr uint32_t backendMask = MLN_PLUGIN_BACKEND_METAL;
#else
#error "Define MLN_SHADOW_ANDROID or MLN_SHADOW_IOS"
#endif

const mln_plugin_layer_extension_v1 layerExtension{sizeof(mln_plugin_layer_extension_v1),
                                                   pluginString(layerType, sizeof(layerType)),
                                                   0,
                                                   backendMask,
                                                   &propertyDescriptor,
                                                   1,
                                                   createInstance,
                                                   destroyInstance,
                                                   prepareFrame,
                                                   renderBeforeLayer,
                                                   contextLost};

const mln_plugin_descriptor_v1 descriptor{sizeof(mln_plugin_descriptor_v1),
                                          MLN_PLUGIN_ABI_VERSION_1,
                                          pluginString(pluginID, sizeof(pluginID)),
                                          pluginString(pluginVersion, sizeof(pluginVersion)),
                                          MLN_PLUGIN_ABI_VERSION_1,
                                          MLN_PLUGIN_ABI_VERSION_1,
                                          &layerExtension,
                                          1,
                                          nullptr,
                                          0};

} // namespace

bool shadowEnabled(const mln_plugin_frame_context_v1* frame) {
    if (!frame) return false;
    for (size_t i = 0; i < frame->property_count; ++i) {
        const auto& property = frame->properties[i];
        if (property.name.size == sizeof(propertyName) - 1 && property.name.data &&
            memcmp(property.name.data, propertyName, sizeof(propertyName) - 1) == 0 &&
            property.value.type == MLN_PLUGIN_VALUE_BOOLEAN) {
            return property.value.data.boolean_value != 0;
        }
    }
    return false;
}

void shadowLog(ShadowInstance* instance, int severity, const char* message) {
    if (!instance || !instance->host || !instance->host->log || !message) return;
    instance->host->log(severity, {message, strlen(message)});
}

extern "C" mln_plugin_status mln_fill_extrusion_shadows_register(
    mln_plugin_register_function_v1 registerFunction,
    char* errorMessage,
    size_t errorMessageCapacity) {
    if (!registerFunction) {
        if (errorMessage && errorMessageCapacity) {
            const char message[] = "MapLibre plugin registration function is unavailable";
            const auto count = errorMessageCapacity - 1 < sizeof(message) - 1 ? errorMessageCapacity - 1
                                                                            : sizeof(message) - 1;
            memcpy(errorMessage, message, count);
            errorMessage[count] = '\0';
        }
        return MLN_PLUGIN_STATUS_NOT_FOUND;
    }
    return registerFunction(&descriptor, errorMessage, errorMessageCapacity);
}

extern "C" uint64_t mln_fill_extrusion_shadows_render_callback_count(void) {
    return __atomic_load_n(&renderCallbackCount, __ATOMIC_RELAXED);
}
