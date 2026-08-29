#import <RectangleLayer/RectangleLayer.h>

#include "rectangle_layer.hpp"

NSErrorDomain const MLNRectangleLayerErrorDomain = @"org.maplibre.rectangle-layer";

@implementation MLNRectangleLayerPlugin

+ (BOOL)registerPluginWithError:(NSError**)error {
    char message[512]{};
    const auto status = mln_rectangle_layer_register(&mln_plugin_register_v1, message, sizeof(message));
    if (status == MLN_PLUGIN_STATUS_OK || status == MLN_PLUGIN_STATUS_ALREADY_REGISTERED) return YES;
    if (error) {
        NSString* description = message[0] ? [NSString stringWithUTF8String:message]
                                           : @"MapLibre rejected the rectangle layer plugin";
        *error = [NSError errorWithDomain:MLNRectangleLayerErrorDomain
                                     code:status
                                 userInfo:@{NSLocalizedDescriptionKey : description}];
    }
    return NO;
}

@end
