#pragma once

#include <mbgl/plugin/plugin_api.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct GltfVertex {
    float position[3];
    float normal[3];
    float color[4];
};

struct GltfModelData {
    std::vector<GltfVertex> vertices;
    std::vector<uint32_t> indices;
};

struct GltfLayerInstance {
    const mln_plugin_host_api_v1* host = nullptr;
    std::string requestedURI;
    uint64_t requestID = 0;
    std::shared_ptr<GltfModelData> model;
    uint64_t modelGeneration = 0;
    void* openGL = nullptr;
    void* vulkan = nullptr;
    void* metal = nullptr;
};

const mln_plugin_property_value_v1* gltfProperty(const mln_plugin_frame_context_v1*, const char* name);
bool gltfModelMatrix(const mln_plugin_frame_context_v1*, float matrix[16], float& opacity);
void gltfLog(GltfLayerInstance*, int severity, const std::string&);

#if defined(MLN_GLTF_ANDROID)
mln_plugin_status gltfRenderOpenGL(GltfLayerInstance*, const mln_plugin_frame_context_v1*);
void gltfDestroyOpenGL(GltfLayerInstance*);
mln_plugin_status gltfRenderVulkan(GltfLayerInstance*, const mln_plugin_frame_context_v1*);
void gltfDestroyVulkan(GltfLayerInstance*);
#elif defined(MLN_GLTF_IOS)
mln_plugin_status gltfRenderMetal(GltfLayerInstance*, const mln_plugin_frame_context_v1*);
void gltfDestroyMetal(GltfLayerInstance*);
#endif

extern "C" mln_plugin_status mln_gltf_layer_register(mln_plugin_register_function_v1,
                                                      char* error_message,
                                                      size_t error_message_capacity);
extern "C" uint64_t mln_gltf_layer_prepare_callback_count(void);
extern "C" uint64_t mln_gltf_layer_load_callback_count(void);
extern "C" uint64_t mln_gltf_layer_render_callback_count(void);
extern "C" uint64_t mln_gltf_layer_loaded_vertex_count(void);
