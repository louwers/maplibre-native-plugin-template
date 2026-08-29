#pragma once

#if __has_include(<MapLibre/MLNPluginAPI.h>)
#include <MapLibre/MLNPluginAPI.h>
#else
#include <mln/plugin/plugin_api.h>
#endif

#include <stddef.h>
#include <stdint.h>

struct ShadowOpenGLState {
    uint32_t maskProgram = 0;
    uint32_t compositeProgram = 0;
    uint32_t framebuffer = 0;
    uint32_t maskTexture = 0;
    uint32_t depthStencil = 0;
    uint32_t vertexArray = 0;
    uint32_t quadBuffer = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct ShadowVulkanState {
    void* resources = nullptr;
};

struct ShadowInstance {
    const mln_plugin_host_api_v1* host = nullptr;
    mln_plugin_backend lastBackend = static_cast<mln_plugin_backend>(0);
    ShadowOpenGLState gl;
    ShadowVulkanState vk;
    void* metalResources = nullptr;
};

bool shadowEnabled(const mln_plugin_frame_context_v1* frame);
void shadowLog(ShadowInstance* instance, int severity, const char* message);

#if defined(MLN_SHADOW_ANDROID)
mln_plugin_status shadowOpenGLPrepare(ShadowInstance*, const mln_plugin_frame_context_v1*);
mln_plugin_status shadowOpenGLRender(ShadowInstance*, const mln_plugin_frame_context_v1*);
void shadowOpenGLContextLost(ShadowInstance*);
void shadowOpenGLDestroy(ShadowInstance*);

mln_plugin_status shadowVulkanPrepare(ShadowInstance*, const mln_plugin_frame_context_v1*);
mln_plugin_status shadowVulkanRender(ShadowInstance*, const mln_plugin_frame_context_v1*);
void shadowVulkanContextLost(ShadowInstance*);
void shadowVulkanDestroy(ShadowInstance*);
#endif

#if defined(MLN_SHADOW_IOS)
mln_plugin_status shadowMetalPrepare(ShadowInstance*, const mln_plugin_frame_context_v1*);
mln_plugin_status shadowMetalRender(ShadowInstance*, const mln_plugin_frame_context_v1*);
void shadowMetalContextLost(ShadowInstance*);
void shadowMetalDestroy(ShadowInstance*);
#endif

#ifdef __cplusplus
extern "C" {
#endif

mln_plugin_status mln_fill_extrusion_shadows_register(mln_plugin_register_function_v1 register_function,
                                                       char* error_message,
                                                       size_t error_message_capacity);
uint64_t mln_fill_extrusion_shadows_render_callback_count(void);

#ifdef __cplusplus
}
#endif
