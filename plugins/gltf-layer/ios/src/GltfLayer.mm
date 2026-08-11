#import <GltfLayer/GltfLayer.h>

#include "gltf_layer.hpp"

NSErrorDomain const MLNGltfLayerErrorDomain = @"org.maplibre.gltf-layer";

@implementation MLNGltfLayerPlugin

+ (BOOL)registerPluginWithError:(NSError**)error {
    char message[512]{};
    const auto status = mln_gltf_layer_register(&mln_plugin_register_v1, message, sizeof(message));
    if (status == MLN_PLUGIN_STATUS_OK || status == MLN_PLUGIN_STATUS_ALREADY_REGISTERED) return YES;
    if (error) {
        NSString* description = message[0] ? [NSString stringWithUTF8String:message]
                                           : @"MapLibre rejected the GLTF layer plugin";
        *error = [NSError errorWithDomain:MLNGltfLayerErrorDomain
                                     code:status
                                 userInfo:@{NSLocalizedDescriptionKey : description}];
    }
    return NO;
}

+ (uint64_t)prepareCallbackCount { return mln_gltf_layer_prepare_callback_count(); }
+ (uint64_t)loadCallbackCount { return mln_gltf_layer_load_callback_count(); }
+ (uint64_t)renderCallbackCount { return mln_gltf_layer_render_callback_count(); }
+ (uint64_t)loadedVertexCount { return mln_gltf_layer_loaded_vertex_count(); }

@end
