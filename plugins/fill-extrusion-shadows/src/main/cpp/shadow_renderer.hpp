#pragma once

#include <mbgl/plugin/plugin_api.h>

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
};

bool shadowEnabled(const mln_plugin_frame_context_v1* frame);
void shadowLog(ShadowInstance* instance, int severity, const char* message);

mln_plugin_status shadowOpenGLPrepare(ShadowInstance*, const mln_plugin_frame_context_v1*);
mln_plugin_status shadowOpenGLRender(ShadowInstance*, const mln_plugin_frame_context_v1*);
void shadowOpenGLContextLost(ShadowInstance*);
void shadowOpenGLDestroy(ShadowInstance*);

mln_plugin_status shadowVulkanPrepare(ShadowInstance*, const mln_plugin_frame_context_v1*);
mln_plugin_status shadowVulkanRender(ShadowInstance*, const mln_plugin_frame_context_v1*);
void shadowVulkanContextLost(ShadowInstance*);
void shadowVulkanDestroy(ShadowInstance*);
