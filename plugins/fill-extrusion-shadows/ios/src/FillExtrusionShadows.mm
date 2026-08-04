#import <FillExtrusionShadows/FillExtrusionShadows.h>

#include "shadow_renderer.hpp"

NSErrorDomain const MLNFillExtrusionShadowsErrorDomain = @"org.maplibre.fill-extrusion-shadows";
NSString *const MLNFillExtrusionShadowProperty = @"fill-extrusion-shadow";

@implementation MLNFillExtrusionShadowsPlugin

+ (BOOL)registerPluginWithError:(NSError **)error {
    char message[512]{};
    const auto status = mln_fill_extrusion_shadows_register(&mln_plugin_register_v1, message, sizeof(message));
    if (status == MLN_PLUGIN_STATUS_OK || status == MLN_PLUGIN_STATUS_ALREADY_REGISTERED) {
        return YES;
    }
    if (error) {
        NSString *description = message[0] ? [NSString stringWithUTF8String:message]
                                           : @"MapLibre rejected the fill-extrusion shadows plugin";
        *error = [NSError errorWithDomain:MLNFillExtrusionShadowsErrorDomain
                                     code:status
                                 userInfo:@{NSLocalizedDescriptionKey : description}];
    }
    return NO;
}

+ (uint64_t)renderCallbackCount {
    return mln_fill_extrusion_shadows_render_callback_count();
}

@end
