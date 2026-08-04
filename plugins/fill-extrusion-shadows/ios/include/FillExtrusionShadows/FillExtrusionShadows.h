#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNFillExtrusionShadowsErrorDomain;
FOUNDATION_EXPORT NSString *const MLNFillExtrusionShadowProperty;

/** Registers the fill-extrusion shadow extension before a dependent style is loaded. */
NS_SWIFT_NAME(FillExtrusionShadowsPlugin)
@interface MLNFillExtrusionShadowsPlugin : NSObject

+ (BOOL)registerPluginWithError:(NSError * _Nullable * _Nullable)error
    NS_SWIFT_NAME(registerPlugin());

+ (uint64_t)renderCallbackCount;

@end

NS_ASSUME_NONNULL_END
