#import <HillshadeLayer/HillshadeLayer.h>

#include "hillshade_layer.hpp"

NSErrorDomain const MLNHillshadeLayerErrorDomain = @"org.maplibre.hillshade";

@implementation MLNHillshadeLayerPlugin

+ (BOOL)registerPluginWithError:(NSError**)error {
    char message[512]{};
    const auto status = mln_hillshade_layer_register(&mln_plugin_register_v1, message, sizeof(message));
    if (status == MLN_PLUGIN_STATUS_OK || status == MLN_PLUGIN_STATUS_ALREADY_REGISTERED) return YES;
    if (error) {
        NSString* description = message[0] ? [NSString stringWithUTF8String:message]
                                           : @"MapLibre rejected the hillshade layer plugin";
        *error = [NSError errorWithDomain:MLNHillshadeLayerErrorDomain
                                     code:status
                                 userInfo:@{NSLocalizedDescriptionKey : description}];
    }
    return NO;
}

@end
