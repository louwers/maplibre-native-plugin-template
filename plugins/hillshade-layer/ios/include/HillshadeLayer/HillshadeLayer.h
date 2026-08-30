#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNHillshadeLayerErrorDomain;

/** Registers the source-bound `org.maplibre.hillshade` style layer before loading a style. */
NS_SWIFT_NAME(HillshadeLayerPlugin)
@interface MLNHillshadeLayerPlugin : NSObject

+ (BOOL)registerPluginWithError:(NSError* _Nullable* _Nullable)error
    NS_SWIFT_NAME(registerPlugin());

@end

NS_ASSUME_NONNULL_END
