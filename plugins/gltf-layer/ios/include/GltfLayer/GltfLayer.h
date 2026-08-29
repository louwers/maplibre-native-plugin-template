#import <Foundation/Foundation.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MLNGltfLayerErrorDomain;

@interface MLNGltfLayerPlugin : NSObject
+ (BOOL)registerPluginWithError:(NSError* _Nullable* _Nullable)error;
@end

NS_ASSUME_NONNULL_END
