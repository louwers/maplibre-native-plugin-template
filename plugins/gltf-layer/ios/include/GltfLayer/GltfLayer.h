#import <Foundation/Foundation.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNGltfLayerErrorDomain;

@interface MLNGltfLayerPlugin : NSObject
+ (BOOL)registerPluginWithError:(NSError* _Nullable* _Nullable)error;
+ (uint64_t)prepareCallbackCount;
+ (uint64_t)loadCallbackCount;
+ (uint64_t)renderCallbackCount;
+ (uint64_t)loadedVertexCount;
@end

NS_ASSUME_NONNULL_END
