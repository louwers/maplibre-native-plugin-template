#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNHeatmapLayerErrorDomain;

/** Registers the source-bound `org.maplibre.heatmap` style layer. */
NS_SWIFT_NAME(HeatmapLayerPlugin)
@interface MLNHeatmapLayerPlugin : NSObject

+ (BOOL)registerPluginWithError:(NSError *_Nullable *_Nullable)error
    NS_SWIFT_NAME(registerPlugin());

@end

NS_ASSUME_NONNULL_END
