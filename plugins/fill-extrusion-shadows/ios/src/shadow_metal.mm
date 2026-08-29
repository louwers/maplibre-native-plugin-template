#import <Metal/Metal.h>
#import <simd/simd.h>

#include "shadow_renderer.hpp"

#include <algorithm>
#include <cstring>

namespace {

NSString *const shaderSource = @R"metal(
#include <metal_stdlib>
using namespace metal;

struct ShadowUniforms {
    float4x4 matrix;
    float constantBase;
    float constantHeight;
    float baseT;
    float heightT;
    float heightFactor;
    uint positionStride;
    uint decimalsStride;
    uint baseStride;
    uint heightStride;
    uint wallStride;
    uint baseAttribute;
    uint heightAttribute;
};

vertex float4 fillExtrusionShadowVertex(
    uint vertexID [[vertex_id]],
    const device uchar *positionBytes [[buffer(0)]],
    const device uchar *decimalsBytes [[buffer(1)]],
    const device uchar *baseBytes [[buffer(2)]],
    const device uchar *heightBytes [[buffer(3)]],
    constant ShadowUniforms &u [[buffer(4)]]) {
    const short2 rawPosition = short2(*reinterpret_cast<const device packed_short2 *>(
        positionBytes + vertexID * u.positionStride));
    const ushort2 rawDecimals = ushort2(*reinterpret_cast<const device packed_ushort2 *>(
        decimalsBytes + vertexID * u.decimalsStride));

    float base = u.constantBase;
    if (u.baseAttribute == 1) {
        base = *reinterpret_cast<const device float *>(baseBytes + vertexID * u.baseStride);
    } else if (u.baseAttribute == 2) {
        const float2 values = float2(*reinterpret_cast<const device packed_float2 *>(
            baseBytes + vertexID * u.baseStride));
        base = mix(values.x, values.y, u.baseT);
    }
    float height = u.constantHeight;
    if (u.heightAttribute == 1) {
        height = *reinterpret_cast<const device float *>(heightBytes + vertexID * u.heightStride);
    } else if (u.heightAttribute == 2) {
        const float2 values = float2(*reinterpret_cast<const device packed_float2 *>(
            heightBytes + vertexID * u.heightStride));
        height = mix(values.x, values.y, u.heightT);
    }
    base = max(0.0f, base);
    height = max(0.0f, height);

    const int packed = int(floor(float(rawDecimals.x) / 2.0f));
    const int first = packed / 256;
    const float2 decimals = float2(first, packed - first * 256) / 128.0f;
    const float upper = float(rawDecimals.x & 1u);
    const float projectionHeight = mix(base, height, upper);
    const float2 direction = float2(-0.5f, -0.5f) * (-u.heightFactor) * 0.38f;
    const float2 projected = float2(rawPosition) + decimals + direction * projectionHeight;
    return u.matrix * float4(projected, 0.0f, 1.0f);
}

vertex float4 fillExtrusionShadowWallVertex(
    uint vertexID [[vertex_id]],
    uint instanceID [[instance_id]],
    const device uchar *wallBytes [[buffer(0)]],
    const device uchar *positionBytes [[buffer(1)]],
    const device uchar *decimalsBytes [[buffer(2)]],
    const device uchar *baseBytes [[buffer(3)]],
    const device uchar *heightBytes [[buffer(4)]],
    constant ShadowUniforms &u [[buffer(5)]]) {
    const short2 wallVertex = short2(*reinterpret_cast<const device packed_short2 *>(
        wallBytes + vertexID * u.wallStride));
    const short2 rawPosition1 = short2(*reinterpret_cast<const device packed_short2 *>(
        positionBytes + instanceID * u.positionStride));
    const short2 rawPosition2 = short2(*reinterpret_cast<const device packed_short2 *>(
        positionBytes + (instanceID + 1u) * u.positionStride));
    const ushort2 rawDecimals1 = ushort2(*reinterpret_cast<const device packed_ushort2 *>(
        decimalsBytes + instanceID * u.decimalsStride));
    const ushort2 rawDecimals2 = ushort2(*reinterpret_cast<const device packed_ushort2 *>(
        decimalsBytes + (instanceID + 1u) * u.decimalsStride));
    if ((rawDecimals1.x & 1u) != 0u) {
        return float4(2.0f, 2.0f, 2.0f, 1.0f);
    }

    float base = u.constantBase;
    if (u.baseAttribute == 1) {
        base = *reinterpret_cast<const device float *>(baseBytes + instanceID * u.baseStride);
    } else if (u.baseAttribute == 2) {
        const float2 values = float2(*reinterpret_cast<const device packed_float2 *>(
            baseBytes + instanceID * u.baseStride));
        base = mix(values.x, values.y, u.baseT);
    }
    float height = u.constantHeight;
    if (u.heightAttribute == 1) {
        height = *reinterpret_cast<const device float *>(heightBytes + instanceID * u.heightStride);
    } else if (u.heightAttribute == 2) {
        const float2 values = float2(*reinterpret_cast<const device packed_float2 *>(
            heightBytes + instanceID * u.heightStride));
        height = mix(values.x, values.y, u.heightT);
    }
    base = max(0.0f, base);
    height = max(0.0f, height);

    const uint packed1 = rawDecimals1.x / 2u;
    const uint packed2 = rawDecimals2.x / 2u;
    const float2 decimals1 = float2(packed1 / 256u, packed1 % 256u) / 128.0f;
    const float2 decimals2 = float2(packed2 / 256u, packed2 % 256u) / 128.0f;
    const float2 position1 = float2(rawPosition1) + decimals1;
    const float2 position2 = float2(rawPosition2) + decimals2;
    const float upper = float(wallVertex.y);
    const float projectionHeight = mix(base, height, upper);
    const float2 direction = float2(-0.5f, -0.5f) * (-u.heightFactor) * 0.38f;
    const float2 projected = (wallVertex.x == 0 ? position1 : position2) + direction * projectionHeight;
    return u.matrix * float4(projected, 0.0f, 1.0f);
}

fragment float4 fillExtrusionShadowMaskFragment() {
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

vertex float4 fillExtrusionShadowCompositeVertex(uint vertexID [[vertex_id]]) {
    constexpr float2 positions[] = {
        float2(-1.0f, -1.0f),
        float2( 3.0f, -1.0f),
        float2(-1.0f,  3.0f),
    };
    return float4(positions[vertexID], 0.0f, 1.0f);
}

fragment float4 fillExtrusionShadowCompositeFragment(
    float4 position [[position]],
    texture2d<float, access::read> mask [[texture(0)]],
    constant float &alpha [[buffer(0)]]) {
    const float coverage = mask.read(uint2(position.xy)).r;
    return float4(0.0f, 0.0f, 0.0f, alpha * coverage);
}
)metal";

struct ShadowUniforms {
    simd_float4x4 matrix;
    float constantBase;
    float constantHeight;
    float baseT;
    float heightT;
    float heightFactor;
    uint32_t positionStride;
    uint32_t decimalsStride;
    uint32_t baseStride;
    uint32_t heightStride;
    uint32_t wallStride;
    uint32_t baseAttribute;
    uint32_t heightAttribute;
};

} // namespace

@interface MLNShadowMetalResources : NSObject
@property(nonatomic, strong) id<MTLRenderPipelineState> maskPipeline;
@property(nonatomic, strong) id<MTLRenderPipelineState> wallMaskPipeline;
@property(nonatomic, strong) id<MTLRenderPipelineState> compositePipeline;
@property(nonatomic, strong) id<MTLDepthStencilState> depthStencil;
@property(nonatomic, strong) id<MTLTexture> maskTexture;
@property(nonatomic) MTLPixelFormat colorFormat;
@property(nonatomic) MTLPixelFormat depthFormat;
@property(nonatomic) MTLPixelFormat stencilFormat;
@property(nonatomic) NSUInteger sampleCount;
@property(nonatomic) NSUInteger width;
@property(nonatomic) NSUInteger height;
@end

@implementation MLNShadowMetalResources
@end

namespace {

MLNShadowMetalResources *resources(ShadowInstance *instance) {
    return instance && instance->metalResources
               ? (__bridge MLNShadowMetalResources *)instance->metalResources
               : nil;
}

void logError(ShadowInstance *instance, NSError *error, const char *fallback) {
    const char *message = error.localizedDescription.UTF8String;
    shadowLog(instance, 3, message ? message : fallback);
}

bool ensureResources(ShadowInstance *instance,
                     const mln_plugin_metal_context_v1 *metal,
                     uint32_t width,
                     uint32_t height) {
    if (!instance || !metal || !metal->device) return false;
    auto *current = resources(instance);
    const auto colorFormat = static_cast<MTLPixelFormat>(metal->color_pixel_format);
    const auto depthFormat = static_cast<MTLPixelFormat>(metal->depth_pixel_format);
    const auto stencilFormat = static_cast<MTLPixelFormat>(metal->stencil_pixel_format);
    const NSUInteger sampleCount = std::max<uint32_t>(metal->sample_count, 1);
    if (current.maskPipeline && current.wallMaskPipeline && current.compositePipeline && current.maskTexture &&
        current.colorFormat == colorFormat && current.depthFormat == depthFormat &&
        current.stencilFormat == stencilFormat && current.sampleCount == sampleCount &&
        current.width == width && current.height == height) {
        return true;
    }

    id<MTLDevice> device = (__bridge id<MTLDevice>)metal->device;
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        logError(instance, error, "Unable to compile fill-extrusion shadow Metal shaders");
        return false;
    }
    id<MTLFunction> maskVertex = [library newFunctionWithName:@"fillExtrusionShadowVertex"];
    id<MTLFunction> wallMaskVertex = [library newFunctionWithName:@"fillExtrusionShadowWallVertex"];
    id<MTLFunction> maskFragment = [library newFunctionWithName:@"fillExtrusionShadowMaskFragment"];
    id<MTLFunction> compositeVertex = [library newFunctionWithName:@"fillExtrusionShadowCompositeVertex"];
    id<MTLFunction> compositeFragment = [library newFunctionWithName:@"fillExtrusionShadowCompositeFragment"];
    if (!maskVertex || !wallMaskVertex || !maskFragment || !compositeVertex || !compositeFragment ||
        colorFormat == MTLPixelFormatInvalid || width == 0 || height == 0) {
        shadowLog(instance, 3, "Unable to load fill-extrusion shadow Metal shader functions");
        return false;
    }

    MTLRenderPipelineDescriptor *maskDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    maskDescriptor.label = @"MapLibre fill-extrusion shadow mask";
    maskDescriptor.vertexFunction = maskVertex;
    maskDescriptor.fragmentFunction = maskFragment;
    maskDescriptor.sampleCount = 1;
    auto *maskColor = maskDescriptor.colorAttachments[0];
    maskColor.pixelFormat = MTLPixelFormatR8Unorm;
    maskColor.writeMask = MTLColorWriteMaskRed;
    maskColor.blendingEnabled = YES;
    maskColor.rgbBlendOperation = MTLBlendOperationMax;
    maskColor.alphaBlendOperation = MTLBlendOperationMax;
    maskColor.sourceRGBBlendFactor = MTLBlendFactorOne;
    maskColor.sourceAlphaBlendFactor = MTLBlendFactorOne;
    maskColor.destinationRGBBlendFactor = MTLBlendFactorOne;
    maskColor.destinationAlphaBlendFactor = MTLBlendFactorOne;

    id<MTLRenderPipelineState> maskPipeline =
        [device newRenderPipelineStateWithDescriptor:maskDescriptor error:&error];
    if (!maskPipeline) {
        logError(instance, error, "Unable to create fill-extrusion shadow mask pipeline");
        return false;
    }

    maskDescriptor.label = @"MapLibre fill-extrusion shadow wall mask";
    maskDescriptor.vertexFunction = wallMaskVertex;
    id<MTLRenderPipelineState> wallMaskPipeline =
        [device newRenderPipelineStateWithDescriptor:maskDescriptor error:&error];
    if (!wallMaskPipeline) {
        logError(instance, error, "Unable to create fill-extrusion shadow wall mask pipeline");
        return false;
    }

    MTLRenderPipelineDescriptor *compositeDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    compositeDescriptor.label = @"MapLibre fill-extrusion shadow composite";
    compositeDescriptor.vertexFunction = compositeVertex;
    compositeDescriptor.fragmentFunction = compositeFragment;
    compositeDescriptor.sampleCount = sampleCount;
    compositeDescriptor.depthAttachmentPixelFormat = depthFormat;
    compositeDescriptor.stencilAttachmentPixelFormat = stencilFormat;
    auto *color = compositeDescriptor.colorAttachments[0];
    color.pixelFormat = colorFormat;
    color.blendingEnabled = YES;
    color.rgbBlendOperation = MTLBlendOperationAdd;
    color.alphaBlendOperation = MTLBlendOperationAdd;
    color.sourceRGBBlendFactor = MTLBlendFactorOne;
    color.sourceAlphaBlendFactor = MTLBlendFactorOne;
    color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    id<MTLRenderPipelineState> compositePipeline =
        [device newRenderPipelineStateWithDescriptor:compositeDescriptor error:&error];
    if (!compositePipeline) {
        logError(instance, error, "Unable to create fill-extrusion shadow composite pipeline");
        return false;
    }

    MTLTextureDescriptor *textureDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    textureDescriptor.storageMode = MTLStorageModePrivate;
    textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    id<MTLTexture> maskTexture = [device newTextureWithDescriptor:textureDescriptor];
    maskTexture.label = @"MapLibre fill-extrusion shadow union mask";
    if (!maskTexture) {
        shadowLog(instance, 3, "Unable to create fill-extrusion shadow mask texture");
        return false;
    }

    MTLDepthStencilDescriptor *depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
    depthDescriptor.depthWriteEnabled = NO;

    if (!current) {
        current = [[MLNShadowMetalResources alloc] init];
        instance->metalResources = (__bridge_retained void *)current;
    }
    current.maskPipeline = maskPipeline;
    current.wallMaskPipeline = wallMaskPipeline;
    current.compositePipeline = compositePipeline;
    current.depthStencil = [device newDepthStencilStateWithDescriptor:depthDescriptor];
    current.maskTexture = maskTexture;
    current.colorFormat = colorFormat;
    current.depthFormat = depthFormat;
    current.stencilFormat = stencilFormat;
    current.sampleCount = sampleCount;
    current.width = width;
    current.height = height;
    return true;
}

id<MTLBuffer> buffer(uint64_t handle) {
    return handle ? (__bridge id<MTLBuffer>)reinterpret_cast<void *>(static_cast<uintptr_t>(handle)) : nil;
}

void renderShadowGeometry(id<MTLRenderCommandEncoder> encoder,
                          MLNShadowMetalResources *state,
                          const mln_plugin_frame_context_v1 *frame) {
    [encoder setCullMode:MTLCullModeNone];
    [encoder setViewport:MTLViewport{0.0, 0.0, static_cast<double>(frame->width), static_cast<double>(frame->height), 0.0, 1.0}];
    [encoder setScissorRect:MTLScissorRect{0, 0, frame->width, frame->height}];

    for (size_t i = 0; i < frame->fill_extrusion_packet_count; ++i) {
        const auto &packet = frame->fill_extrusion_packets[i];
        const bool wall = packet.kind == MLN_PLUGIN_DRAW_PACKET_INSTANCED_WALLS;
        if ((packet.kind != MLN_PLUGIN_DRAW_PACKET_TRIANGLES && !wall) || !packet.index_buffer ||
            !packet.position.buffer || !packet.decimals_edge.buffer || (wall && !packet.wall_vertex.buffer)) {
            continue;
        }
        id<MTLBuffer> position = buffer(packet.position.buffer);
        id<MTLBuffer> decimals = buffer(packet.decimals_edge.buffer);
        id<MTLBuffer> index = buffer(packet.index_buffer);
        if (!position || !decimals || !index) continue;

        const bool baseAttribute = packet.base_is_attribute && packet.base.buffer &&
                                   (packet.base.type == MLN_PLUGIN_ATTRIBUTE_FLOAT ||
                                    packet.base.type == MLN_PLUGIN_ATTRIBUTE_FLOAT_X2);
        const bool heightAttribute = packet.height_is_attribute && packet.height.buffer &&
                                     (packet.height.type == MLN_PLUGIN_ATTRIBUTE_FLOAT ||
                                      packet.height.type == MLN_PLUGIN_ATTRIBUTE_FLOAT_X2);
        id<MTLBuffer> base = baseAttribute ? buffer(packet.base.buffer) : position;
        id<MTLBuffer> height = heightAttribute ? buffer(packet.height.buffer) : position;

        ShadowUniforms uniforms{};
        std::memcpy(&uniforms.matrix, packet.tile_matrix, sizeof(packet.tile_matrix));
        uniforms.constantBase = packet.constant_base;
        uniforms.constantHeight = packet.constant_height;
        uniforms.baseT = packet.base_interpolation;
        uniforms.heightT = packet.height_interpolation;
        uniforms.heightFactor = packet.height_factor;
        uniforms.positionStride = packet.position.stride;
        uniforms.decimalsStride = packet.decimals_edge.stride;
        uniforms.baseStride = baseAttribute ? packet.base.stride : packet.position.stride;
        uniforms.heightStride = heightAttribute ? packet.height.stride : packet.position.stride;
        uniforms.wallStride = packet.wall_vertex.stride;
        uniforms.baseAttribute = !baseAttribute ? 0 : packet.base.type == MLN_PLUGIN_ATTRIBUTE_FLOAT ? 1 : 2;
        uniforms.heightAttribute = !heightAttribute ? 0 : packet.height.type == MLN_PLUGIN_ATTRIBUTE_FLOAT ? 1 : 2;

        if (wall) {
            [encoder setRenderPipelineState:state.wallMaskPipeline];
            [encoder setVertexBuffer:buffer(packet.wall_vertex.buffer) offset:packet.wall_vertex.offset atIndex:0];
            [encoder setVertexBuffer:position offset:packet.position.offset atIndex:1];
            [encoder setVertexBuffer:decimals offset:packet.decimals_edge.offset atIndex:2];
            [encoder setVertexBuffer:base offset:baseAttribute ? packet.base.offset : packet.position.offset atIndex:3];
            [encoder setVertexBuffer:height
                              offset:heightAttribute ? packet.height.offset : packet.position.offset
                             atIndex:4];
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:5];
        } else {
            [encoder setRenderPipelineState:state.maskPipeline];
            [encoder setVertexBuffer:position offset:packet.position.offset atIndex:0];
            [encoder setVertexBuffer:decimals offset:packet.decimals_edge.offset atIndex:1];
            [encoder setVertexBuffer:base offset:baseAttribute ? packet.base.offset : packet.position.offset atIndex:2];
            [encoder setVertexBuffer:height
                              offset:heightAttribute ? packet.height.offset : packet.position.offset
                             atIndex:3];
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:4];
        }
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:packet.index_count
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:index
                     indexBufferOffset:packet.index_offset
                         instanceCount:wall ? std::max<uint32_t>(packet.instance_count, 1u) : 1u
                            baseVertex:packet.base_vertex
                          baseInstance:0];
    }
}

} // namespace

mln_plugin_status shadowMetalPrepare(ShadowInstance *instance, const mln_plugin_frame_context_v1 *frame) {
    if (!instance || !frame || !frame->backend || !frame->backend->metal) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    const auto *metal = frame->backend->metal;
    if (!metal->command_buffer || !ensureResources(instance, metal, frame->width, frame->height)) {
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    auto *state = resources(instance);
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = state.maskTexture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

    id<MTLCommandBuffer> commandBuffer = (__bridge id<MTLCommandBuffer>)metal->command_buffer;
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (!encoder) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    [encoder pushDebugGroup:@"MapLibre fill-extrusion shadow union mask"];
    renderShadowGeometry(encoder, state, frame);
    [encoder popDebugGroup];
    [encoder endEncoding];
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status shadowMetalRender(ShadowInstance *instance, const mln_plugin_frame_context_v1 *frame) {
    if (!instance || !frame || !frame->backend || !frame->backend->metal) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    const auto *metal = frame->backend->metal;
    if (!metal->render_command_encoder || !ensureResources(instance, metal, frame->width, frame->height)) {
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    auto *state = resources(instance);
    id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)metal->render_command_encoder;
    [encoder pushDebugGroup:@"MapLibre fill-extrusion shadows"];
    [encoder setRenderPipelineState:state.compositePipeline];
    [encoder setDepthStencilState:state.depthStencil];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setViewport:MTLViewport{0.0, 0.0, static_cast<double>(frame->width), static_cast<double>(frame->height), 0.0, 1.0}];
    [encoder setScissorRect:MTLScissorRect{0, 0, frame->width, frame->height}];
    float alpha = 0.0f;
    for (size_t i = 0; i < frame->fill_extrusion_packet_count; ++i) {
        const auto &packet = frame->fill_extrusion_packets[i];
        if (packet.kind == MLN_PLUGIN_DRAW_PACKET_TRIANGLES) {
            alpha = 0.35f * packet.layer_opacity;
            break;
        }
    }
    [encoder setFragmentTexture:state.maskTexture atIndex:0];
    [encoder setFragmentBytes:&alpha length:sizeof(alpha) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder popDebugGroup];
    return MLN_PLUGIN_STATUS_OK;
}

void shadowMetalContextLost(ShadowInstance *instance) {
    shadowMetalDestroy(instance);
}

void shadowMetalDestroy(ShadowInstance *instance) {
    if (!instance || !instance->metalResources) return;
    CFBridgingRelease(instance->metalResources);
    instance->metalResources = nullptr;
}
