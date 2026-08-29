#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNRectangleLayerErrorDomain;

/** Registers the source-bound `rectangle` style layer before a style is loaded. */
NS_SWIFT_NAME(RectangleLayerPlugin)
@interface MLNRectangleLayerPlugin : NSObject

+ (BOOL)registerPluginWithError:(NSError* _Nullable* _Nullable)error
    NS_SWIFT_NAME(registerPlugin());

@end

NS_ASSUME_NONNULL_END
