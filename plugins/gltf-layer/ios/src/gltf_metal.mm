#import <Metal/Metal.h>
#import <simd/simd.h>

#include "gltf_layer.hpp"

#include <algorithm>
#include <cstring>

namespace {

NSString* const shaderSource = @R"metal(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float4 color [[attribute(2)]];
};
struct Uniforms { float4x4 matrix; float opacity; };
struct VertexOut { float4 position [[position]]; float3 normal; float4 color; };

vertex VertexOut gltfVertex(VertexIn in [[stage_in]], constant Uniforms& u [[buffer(1)]]) {
    VertexOut out;
    out.position = u.matrix * float4(in.position, 1.0);
    out.normal = normalize(float3(in.normal.x, -in.normal.z, in.normal.y));
    out.color = in.color;
    return out;
}

fragment float4 gltfFragment(VertexOut in [[stage_in]], constant Uniforms& u [[buffer(1)]]) {
    const float3 light = normalize(float3(-0.45, -0.55, 0.75));
    const float diffuse = 0.35 + 0.65 * abs(dot(normalize(in.normal), light));
    const float alpha = clamp(in.color.a * u.opacity, 0.0, 1.0);
    return float4(in.color.rgb * diffuse * alpha, alpha);
}
)metal";

struct Uniforms {
    simd_float4x4 matrix;
    float opacity;
};

} // namespace

@interface MLNGltfMetalResources : NSObject
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@property(nonatomic, strong) id<MTLDepthStencilState> depthStencil;
@property(nonatomic, strong) id<MTLBuffer> vertices;
@property(nonatomic, strong) id<MTLBuffer> indices;
@property(nonatomic) MTLPixelFormat colorFormat;
@property(nonatomic) MTLPixelFormat depthFormat;
@property(nonatomic) MTLPixelFormat stencilFormat;
@property(nonatomic) NSUInteger sampleCount;
@property(nonatomic) uint64_t generation;
@end

@implementation MLNGltfMetalResources
@end

namespace {

MLNGltfMetalResources* resources(GltfLayerInstance* instance) {
    return instance && instance->metal ? (__bridge MLNGltfMetalResources*)instance->metal : nil;
}

bool ensurePipeline(GltfLayerInstance* instance, const mln_plugin_metal_context_v1* metal) {
    if (!instance || !instance->model || !metal || !metal->device) return false;
    id<MTLDevice> device = (__bridge id<MTLDevice>)metal->device;
    auto* state = resources(instance);
    if (!state) {
        state = [[MLNGltfMetalResources alloc] init];
        instance->metal = (__bridge_retained void*)state;
    }
    const auto colorFormat = static_cast<MTLPixelFormat>(metal->color_pixel_format);
    const auto depthFormat = static_cast<MTLPixelFormat>(metal->depth_pixel_format);
    const auto stencilFormat = static_cast<MTLPixelFormat>(metal->stencil_pixel_format);
    const NSUInteger sampleCount = std::max<uint32_t>(metal->sample_count, 1);
    if (!state.pipeline || state.colorFormat != colorFormat || state.depthFormat != depthFormat ||
        state.stencilFormat != stencilFormat || state.sampleCount != sampleCount) {
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
        if (!library) {
            gltfLog(instance, 3, error.localizedDescription.UTF8String ?: "Unable to compile GLTF Metal shaders");
            return false;
        }
        MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        descriptor.label = @"MapLibre GLTF layer";
        descriptor.vertexFunction = [library newFunctionWithName:@"gltfVertex"];
        descriptor.fragmentFunction = [library newFunctionWithName:@"gltfFragment"];
        descriptor.sampleCount = sampleCount;
        descriptor.depthAttachmentPixelFormat = depthFormat;
        descriptor.stencilAttachmentPixelFormat = stencilFormat;
        auto* color = descriptor.colorAttachments[0];
        color.pixelFormat = colorFormat;
        color.blendingEnabled = YES;
        color.rgbBlendOperation = MTLBlendOperationAdd;
        color.alphaBlendOperation = MTLBlendOperationAdd;
        color.sourceRGBBlendFactor = MTLBlendFactorOne;
        color.sourceAlphaBlendFactor = MTLBlendFactorOne;
        color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        MTLVertexDescriptor* vertices = [[MTLVertexDescriptor alloc] init];
        vertices.attributes[0].format = MTLVertexFormatFloat3;
        vertices.attributes[0].offset = offsetof(GltfVertex, position);
        vertices.attributes[0].bufferIndex = 0;
        vertices.attributes[1].format = MTLVertexFormatFloat3;
        vertices.attributes[1].offset = offsetof(GltfVertex, normal);
        vertices.attributes[1].bufferIndex = 0;
        vertices.attributes[2].format = MTLVertexFormatFloat4;
        vertices.attributes[2].offset = offsetof(GltfVertex, color);
        vertices.attributes[2].bufferIndex = 0;
        vertices.layouts[0].stride = sizeof(GltfVertex);
        vertices.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        descriptor.vertexDescriptor = vertices;

        state.pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!state.pipeline) {
            gltfLog(instance, 3, error.localizedDescription.UTF8String ?: "Unable to create GLTF Metal pipeline");
            return false;
        }
        MTLDepthStencilDescriptor* depth = [[MTLDepthStencilDescriptor alloc] init];
        depth.depthCompareFunction = MTLCompareFunctionLessEqual;
        depth.depthWriteEnabled = YES;
        state.depthStencil = [device newDepthStencilStateWithDescriptor:depth];
        state.colorFormat = colorFormat;
        state.depthFormat = depthFormat;
        state.stencilFormat = stencilFormat;
        state.sampleCount = sampleCount;
    }
    if (state.generation != instance->modelGeneration) {
        state.vertices = [device newBufferWithBytes:instance->model->vertices.data()
                                             length:instance->model->vertices.size() * sizeof(GltfVertex)
                                            options:MTLResourceStorageModeShared];
        state.indices = [device newBufferWithBytes:instance->model->indices.data()
                                            length:instance->model->indices.size() * sizeof(uint32_t)
                                           options:MTLResourceStorageModeShared];
        state.generation = instance->modelGeneration;
    }
    return state.vertices && state.indices;
}

} // namespace

mln_plugin_status gltfRenderMetal(GltfLayerInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !frame || !frame->backend || !frame->backend->metal || !instance->model) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    const auto* metal = frame->backend->metal;
    if (!metal->render_command_encoder || !ensurePipeline(instance, metal)) {
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    float matrix[16];
    float opacity = 1.0f;
    if (!gltfModelMatrix(frame, matrix, opacity) || opacity <= 0.0f) return MLN_PLUGIN_STATUS_OK;
    Uniforms uniforms{};
    std::memcpy(&uniforms.matrix, matrix, sizeof(matrix));
    uniforms.opacity = opacity;

    auto* state = resources(instance);
    id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)metal->render_command_encoder;
    [encoder pushDebugGroup:@"MapLibre GLTF layer"];
    [encoder setRenderPipelineState:state.pipeline];
    [encoder setDepthStencilState:state.depthStencil];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setViewport:MTLViewport{0.0, 0.0, static_cast<double>(frame->width), static_cast<double>(frame->height), 0.0, 1.0}];
    [encoder setScissorRect:MTLScissorRect{0, 0, frame->width, frame->height}];
    [encoder setVertexBuffer:state.vertices offset:0 atIndex:0];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:instance->model->indices.size()
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:state.indices
                 indexBufferOffset:0];
    [encoder popDebugGroup];
    return MLN_PLUGIN_STATUS_OK;
}

void gltfDestroyMetal(GltfLayerInstance* instance) {
    if (!instance || !instance->metal) return;
    CFBridgingRelease(instance->metal);
    instance->metal = nullptr;
}
