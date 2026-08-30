#import <HeatmapLayer/HeatmapLayer.h>

#include "heatmap_layer.hpp"

NSErrorDomain const MLNHeatmapLayerErrorDomain = @"org.maplibre.heatmap";

@implementation MLNHeatmapLayerPlugin

+ (BOOL)registerPluginWithError:(NSError **)error {
  char message[512]{};
  const auto status = mln_heatmap_layer_register(&mln_plugin_register_v1, message, sizeof(message));
  if (status == MLN_PLUGIN_STATUS_OK || status == MLN_PLUGIN_STATUS_ALREADY_REGISTERED) return YES;
  if (error) {
    NSString *description = message[0] ? [NSString stringWithUTF8String:message]
                                       : @"Heatmap plugin registration failed";
    *error = [NSError errorWithDomain:MLNHeatmapLayerErrorDomain
                                 code:status
                             userInfo:@{NSLocalizedDescriptionKey : description}];
  }
  return NO;
}

@end
