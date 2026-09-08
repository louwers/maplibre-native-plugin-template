#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNNgonLayerErrorDomain;

/** Registers the source-bound `ngon` style layer before a style is loaded. */
NS_SWIFT_NAME(NgonLayerPlugin)
@interface MLNNgonLayerPlugin : NSObject

+ (BOOL)registerPluginWithError:(NSError* _Nullable* _Nullable)error
    NS_SWIFT_NAME(registerPlugin());

@end

NS_ASSUME_NONNULL_END
