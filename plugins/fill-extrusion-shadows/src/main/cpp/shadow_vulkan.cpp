#include "shadow_renderer.hpp"

#include <vulkan/vulkan.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "shaders/shadow_composite.frag.spv.inc"
#include "shaders/shadow_composite.vert.spv.inc"
#include "shaders/shadow_mask.frag.spv.inc"
#include "shaders/shadow_roof_b0_h0.vert.spv.inc"
#include "shaders/shadow_roof_b0_h1.vert.spv.inc"
#include "shaders/shadow_roof_b1_h0.vert.spv.inc"
#include "shaders/shadow_roof_b1_h1.vert.spv.inc"
#include "shaders/shadow_wall_b0_h0.vert.spv.inc"
#include "shaders/shadow_wall_b0_h1.vert.spv.inc"
#include "shaders/shadow_wall_b1_h0.vert.spv.inc"
#include "shaders/shadow_wall_b1_h1.vert.spv.inc"

namespace {

VkPhysicalDevice physicalDeviceHandle(uint64_t value) {
    return reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(value));
}

VkDevice deviceHandle(uint64_t value) {
    return reinterpret_cast<VkDevice>(static_cast<uintptr_t>(value));
}

VkCommandBuffer commandBufferHandle(uint64_t value) {
    return reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(value));
}

VkBuffer bufferHandle(uint64_t value) {
#if VK_USE_64_BIT_PTR_DEFINES
    return reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(value));
#else
    return static_cast<VkBuffer>(value);
#endif
}

VkRenderPass renderPassHandle(uint64_t value) {
#if VK_USE_64_BIT_PTR_DEFINES
    return reinterpret_cast<VkRenderPass>(static_cast<uintptr_t>(value));
#else
    return static_cast<VkRenderPass>(value);
#endif
}

constexpr size_t MaxMaskPipelines = 32;

struct Functions {
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    PFN_vkBindImageMemory bindImageMemory = nullptr;
    PFN_vkCreateImageView createImageView = nullptr;
    PFN_vkDestroyImageView destroyImageView = nullptr;
    PFN_vkCreateRenderPass createRenderPass = nullptr;
    PFN_vkDestroyRenderPass destroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer createFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer destroyFramebuffer = nullptr;
    PFN_vkCreateSampler createSampler = nullptr;
    PFN_vkDestroySampler destroySampler = nullptr;
    PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool createDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool destroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets allocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets updateDescriptorSets = nullptr;
    PFN_vkCreateShaderModule createShaderModule = nullptr;
    PFN_vkDestroyShaderModule destroyShaderModule = nullptr;
    PFN_vkCreatePipelineLayout createPipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout destroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines createGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    PFN_vkDeviceWaitIdle deviceWaitIdle = nullptr;
    PFN_vkCmdBeginRenderPass cmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass cmdEndRenderPass = nullptr;
    PFN_vkCmdBindPipeline cmdBindPipeline = nullptr;
    PFN_vkCmdSetViewport cmdSetViewport = nullptr;
    PFN_vkCmdSetScissor cmdSetScissor = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer = nullptr;
    PFN_vkCmdPushConstants cmdPushConstants = nullptr;
    PFN_vkCmdDrawIndexed cmdDrawIndexed = nullptr;
    PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets = nullptr;
    PFN_vkCmdDraw cmdDraw = nullptr;
};

struct ShadowPush {
    float matrix[16];
    float constantBase;
    float constantHeight;
    float baseInterpolation;
    float heightInterpolation;
    float heightFactor;
    float alpha;
    uint32_t baseAttribute;
    uint32_t heightAttribute;
};

static_assert(sizeof(ShadowPush) == 96);

struct PipelineEntry {
    mln_plugin_draw_packet_kind kind{};
    mln_plugin_attribute_type positionType{};
    mln_plugin_attribute_type decimalsType{};
    mln_plugin_attribute_type baseType{};
    mln_plugin_attribute_type heightType{};
    uint32_t wallStride = 0;
    uint32_t positionStride = 0;
    uint32_t decimalsStride = 0;
    uint32_t baseStride = 0;
    uint32_t heightStride = 0;
    uint8_t baseAttribute = 0;
    uint8_t heightAttribute = 0;
    VkPipeline pipeline = VK_NULL_HANDLE;

    bool matches(const mln_plugin_draw_packet_v1& packet) const {
        return kind == packet.kind && positionType == packet.position.type &&
               decimalsType == packet.decimals_edge.type && baseType == packet.base.type &&
               heightType == packet.height.type && wallStride == packet.wall_vertex.stride &&
               positionStride == packet.position.stride && decimalsStride == packet.decimals_edge.stride &&
               baseStride == packet.base.stride && heightStride == packet.height.stride &&
               baseAttribute == packet.base_is_attribute && heightAttribute == packet.height_is_attribute;
    }
};

struct VulkanResources {
    Functions fn;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;

    VkImage maskImage = VK_NULL_HANDLE;
    VkDeviceMemory maskMemory = VK_NULL_HANDLE;
    VkImageView maskView = VK_NULL_HANDLE;
    VkRenderPass maskRenderPass = VK_NULL_HANDLE;
    VkFramebuffer maskFramebuffer = VK_NULL_HANDLE;
    VkSampler maskSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout compositeSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet compositeSet = VK_NULL_HANDLE;
    VkPipelineLayout maskPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout compositePipelineLayout = VK_NULL_HANDLE;
    VkShaderModule roofVertex[4]{};
    VkShaderModule wallVertex[4]{};
    VkShaderModule maskFragment = VK_NULL_HANDLE;
    VkShaderModule compositeVertex = VK_NULL_HANDLE;
    VkShaderModule compositeFragment = VK_NULL_HANDLE;
    PipelineEntry maskPipelines[MaxMaskPipelines]{};
    size_t maskPipelineCount = 0;
    VkPipeline compositePipeline = VK_NULL_HANDLE;
    VkRenderPass compositeRenderPass = VK_NULL_HANDLE;
    const char* failure = nullptr;
};

bool fail(VulkanResources& resources, const char* message) {
    resources.failure = message;
    return false;
}

template <class T>
bool loadProc(T& output, const mln_plugin_backend_context_v1& backend, const char* name) {
    output = reinterpret_cast<T>(backend.get_proc_address(backend.resolver_context, name));
    return output != nullptr;
}

bool loadFunctions(Functions& fn, const mln_plugin_backend_context_v1& backend) {
#define LOAD(member, name) loadProc(fn.member, backend, #name)
    return LOAD(getPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceMemoryProperties) &&
           LOAD(createImage, vkCreateImage) && LOAD(destroyImage, vkDestroyImage) &&
           LOAD(getImageMemoryRequirements, vkGetImageMemoryRequirements) && LOAD(allocateMemory, vkAllocateMemory) &&
           LOAD(freeMemory, vkFreeMemory) && LOAD(bindImageMemory, vkBindImageMemory) &&
           LOAD(createImageView, vkCreateImageView) && LOAD(destroyImageView, vkDestroyImageView) &&
           LOAD(createRenderPass, vkCreateRenderPass) && LOAD(destroyRenderPass, vkDestroyRenderPass) &&
           LOAD(createFramebuffer, vkCreateFramebuffer) && LOAD(destroyFramebuffer, vkDestroyFramebuffer) &&
           LOAD(createSampler, vkCreateSampler) && LOAD(destroySampler, vkDestroySampler) &&
           LOAD(createDescriptorSetLayout, vkCreateDescriptorSetLayout) &&
           LOAD(destroyDescriptorSetLayout, vkDestroyDescriptorSetLayout) &&
           LOAD(createDescriptorPool, vkCreateDescriptorPool) && LOAD(destroyDescriptorPool, vkDestroyDescriptorPool) &&
           LOAD(allocateDescriptorSets, vkAllocateDescriptorSets) &&
           LOAD(updateDescriptorSets, vkUpdateDescriptorSets) && LOAD(createShaderModule, vkCreateShaderModule) &&
           LOAD(destroyShaderModule, vkDestroyShaderModule) && LOAD(createPipelineLayout, vkCreatePipelineLayout) &&
           LOAD(destroyPipelineLayout, vkDestroyPipelineLayout) &&
           LOAD(createGraphicsPipelines, vkCreateGraphicsPipelines) && LOAD(destroyPipeline, vkDestroyPipeline) &&
           LOAD(deviceWaitIdle, vkDeviceWaitIdle) && LOAD(cmdBeginRenderPass, vkCmdBeginRenderPass) &&
           LOAD(cmdEndRenderPass, vkCmdEndRenderPass) && LOAD(cmdBindPipeline, vkCmdBindPipeline) &&
           LOAD(cmdSetViewport, vkCmdSetViewport) && LOAD(cmdSetScissor, vkCmdSetScissor) &&
           LOAD(cmdBindVertexBuffers, vkCmdBindVertexBuffers) && LOAD(cmdBindIndexBuffer, vkCmdBindIndexBuffer) &&
           LOAD(cmdPushConstants, vkCmdPushConstants) && LOAD(cmdDrawIndexed, vkCmdDrawIndexed) &&
           LOAD(cmdBindDescriptorSets, vkCmdBindDescriptorSets) && LOAD(cmdDraw, vkCmdDraw);
#undef LOAD
}

uint32_t memoryType(const VulkanResources& resources, uint32_t allowed, VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties properties{};
    resources.fn.getPhysicalDeviceMemoryProperties(resources.physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((allowed & (1u << i)) && (properties.memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool createImage(VulkanResources& resources,
                 VkFormat format,
                 VkImageUsageFlags usage,
                 VkImageAspectFlags aspect,
                 const char* createFailure,
                 const char* memoryFailure,
                 const char* viewFailure,
                 VkImage& image,
                 VkDeviceMemory& memory,
                 VkImageView& view) {
    const VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                      nullptr,
                                      0,
                                      VK_IMAGE_TYPE_2D,
                                      format,
                                      {resources.width, resources.height, 1},
                                      1,
                                      1,
                                      VK_SAMPLE_COUNT_1_BIT,
                                      VK_IMAGE_TILING_OPTIMAL,
                                      usage,
                                      VK_SHARING_MODE_EXCLUSIVE,
                                      0,
                                      nullptr,
                                      VK_IMAGE_LAYOUT_UNDEFINED};
    if (resources.fn.createImage(resources.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        return fail(resources, createFailure);
    }
    VkMemoryRequirements requirements{};
    resources.fn.getImageMemoryRequirements(resources.device, image, &requirements);
    const uint32_t type = memoryType(resources, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type == UINT32_MAX) return fail(resources, memoryFailure);
    const VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size, type};
    if (resources.fn.allocateMemory(resources.device, &allocateInfo, nullptr, &memory) != VK_SUCCESS ||
        resources.fn.bindImageMemory(resources.device, image, memory, 0) != VK_SUCCESS) {
        return fail(resources, memoryFailure);
    }
    const VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         nullptr,
                                         0,
                                         image,
                                         VK_IMAGE_VIEW_TYPE_2D,
                                         format,
                                         {},
                                         {aspect, 0, 1, 0, 1}};
    if (resources.fn.createImageView(resources.device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        return fail(resources, viewFailure);
    }
    return true;
}

VkShaderModule createShader(VulkanResources& resources, const unsigned char* bytes, size_t size) {
    const VkShaderModuleCreateInfo info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, size, reinterpret_cast<const uint32_t*>(bytes)};
    VkShaderModule module = VK_NULL_HANDLE;
    return resources.fn.createShaderModule(resources.device, &info, nullptr, &module) == VK_SUCCESS ? module
                                                                                                    : VK_NULL_HANDLE;
}

void destroyResources(VulkanResources* resources) {
    if (!resources) return;
    auto& fn = resources->fn;
    const auto device = resources->device;
    if (device) {
        if (resources->compositePipeline) fn.destroyPipeline(device, resources->compositePipeline, nullptr);
        for (size_t i = 0; i < resources->maskPipelineCount; ++i) {
            if (resources->maskPipelines[i].pipeline) {
                fn.destroyPipeline(device, resources->maskPipelines[i].pipeline, nullptr);
            }
        }
        for (size_t i = 0; i < 4; ++i) {
            if (resources->roofVertex[i]) fn.destroyShaderModule(device, resources->roofVertex[i], nullptr);
            if (resources->wallVertex[i]) fn.destroyShaderModule(device, resources->wallVertex[i], nullptr);
        }
        if (resources->maskFragment) fn.destroyShaderModule(device, resources->maskFragment, nullptr);
        if (resources->compositeVertex) fn.destroyShaderModule(device, resources->compositeVertex, nullptr);
        if (resources->compositeFragment) fn.destroyShaderModule(device, resources->compositeFragment, nullptr);
        if (resources->maskPipelineLayout) fn.destroyPipelineLayout(device, resources->maskPipelineLayout, nullptr);
        if (resources->compositePipelineLayout) {
            fn.destroyPipelineLayout(device, resources->compositePipelineLayout, nullptr);
        }
        if (resources->descriptorPool) fn.destroyDescriptorPool(device, resources->descriptorPool, nullptr);
        if (resources->compositeSetLayout) {
            fn.destroyDescriptorSetLayout(device, resources->compositeSetLayout, nullptr);
        }
        if (resources->maskSampler) fn.destroySampler(device, resources->maskSampler, nullptr);
        if (resources->maskFramebuffer) fn.destroyFramebuffer(device, resources->maskFramebuffer, nullptr);
        if (resources->maskRenderPass) fn.destroyRenderPass(device, resources->maskRenderPass, nullptr);
        if (resources->maskView) fn.destroyImageView(device, resources->maskView, nullptr);
        if (resources->maskImage) fn.destroyImage(device, resources->maskImage, nullptr);
        if (resources->maskMemory) fn.freeMemory(device, resources->maskMemory, nullptr);
    }
    free(resources);
}

bool createMaskRenderPass(VulkanResources& resources) {
    const VkAttachmentDescription attachment{0,
                                             VK_FORMAT_R8_UNORM,
                                             VK_SAMPLE_COUNT_1_BIT,
                                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                                             VK_ATTACHMENT_STORE_OP_STORE,
                                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                             VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription subpass{
        0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &colorReference, nullptr, nullptr, 0, nullptr};
    const VkSubpassDependency dependencies[2]{
        {VK_SUBPASS_EXTERNAL,
         0,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         0},
        {0,
         VK_SUBPASS_EXTERNAL,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         0},
    };
    const VkRenderPassCreateInfo info{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, &attachment, 1, &subpass, 2, dependencies};
    return resources.fn.createRenderPass(resources.device, &info, nullptr, &resources.maskRenderPass) == VK_SUCCESS;
}

bool createLayouts(VulkanResources& resources) {
    const VkPushConstantRange maskPush{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPush)};
    const VkPipelineLayoutCreateInfo maskLayoutInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &maskPush};
    if (resources.fn.createPipelineLayout(resources.device, &maskLayoutInfo, nullptr, &resources.maskPipelineLayout) !=
        VK_SUCCESS) {
        return false;
    }

    const VkDescriptorSetLayoutBinding binding{
        0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    const VkDescriptorSetLayoutCreateInfo setInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &binding};
    if (resources.fn.createDescriptorSetLayout(resources.device, &setInfo, nullptr, &resources.compositeSetLayout) !=
        VK_SUCCESS) {
        return false;
    }
    const VkPushConstantRange compositePush{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)};
    const VkPipelineLayoutCreateInfo compositeLayoutInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &resources.compositeSetLayout, 1, &compositePush};
    return resources.fn.createPipelineLayout(
               resources.device, &compositeLayoutInfo, nullptr, &resources.compositePipelineLayout) == VK_SUCCESS;
}

bool createDescriptor(VulkanResources& resources) {
    const VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_FILTER_LINEAR,
                                          VK_FILTER_LINEAR,
                                          VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          0.0f,
                                          VK_FALSE,
                                          1.0f,
                                          VK_FALSE,
                                          VK_COMPARE_OP_ALWAYS,
                                          0.0f,
                                          0.0f,
                                          VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                          VK_FALSE};
    if (resources.fn.createSampler(resources.device, &samplerInfo, nullptr, &resources.maskSampler) != VK_SUCCESS) {
        return false;
    }
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    const VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &poolSize};
    if (resources.fn.createDescriptorPool(resources.device, &poolInfo, nullptr, &resources.descriptorPool) !=
        VK_SUCCESS) {
        return false;
    }
    const VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   nullptr,
                                                   resources.descriptorPool,
                                                   1,
                                                   &resources.compositeSetLayout};
    if (resources.fn.allocateDescriptorSets(resources.device, &allocateInfo, &resources.compositeSet) != VK_SUCCESS) {
        return false;
    }
    const VkDescriptorImageInfo imageInfo{
        resources.maskSampler, resources.maskView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     nullptr,
                                     resources.compositeSet,
                                     0,
                                     0,
                                     1,
                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     &imageInfo,
                                     nullptr,
                                     nullptr};
    resources.fn.updateDescriptorSets(resources.device, 1, &write, 0, nullptr);
    return true;
}

bool createResources(VulkanResources& resources) {
    if (!createImage(resources,
                     VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     "failed to create the Vulkan shadow mask image",
                     "failed to allocate the Vulkan shadow mask image",
                     "failed to create the Vulkan shadow mask image view",
                     resources.maskImage,
                     resources.maskMemory,
                     resources.maskView)) {
        return false;
    }
    if (!createMaskRenderPass(resources)) {
        return fail(resources, "failed to create the Vulkan shadow mask render pass");
    }
    const VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                  nullptr,
                                                  0,
                                                  resources.maskRenderPass,
                                                  1,
                                                  &resources.maskView,
                                                  resources.width,
                                                  resources.height,
                                                  1};
    if (resources.fn.createFramebuffer(resources.device, &framebufferInfo, nullptr, &resources.maskFramebuffer) !=
        VK_SUCCESS) {
        return fail(resources, "failed to create the Vulkan shadow framebuffer");
    }
    if (!createLayouts(resources)) return fail(resources, "failed to create Vulkan shadow pipeline layouts");
    if (!createDescriptor(resources)) return fail(resources, "failed to create Vulkan shadow descriptors");
    struct ShaderBytes {
        const unsigned char* data;
        size_t size;
    };
    const ShaderBytes roofShaders[4]{
        {shadow_roof_b0_h0_vert_spv, shadow_roof_b0_h0_vert_spv_len},
        {shadow_roof_b1_h0_vert_spv, shadow_roof_b1_h0_vert_spv_len},
        {shadow_roof_b0_h1_vert_spv, shadow_roof_b0_h1_vert_spv_len},
        {shadow_roof_b1_h1_vert_spv, shadow_roof_b1_h1_vert_spv_len},
    };
    const ShaderBytes wallShaders[4]{
        {shadow_wall_b0_h0_vert_spv, shadow_wall_b0_h0_vert_spv_len},
        {shadow_wall_b1_h0_vert_spv, shadow_wall_b1_h0_vert_spv_len},
        {shadow_wall_b0_h1_vert_spv, shadow_wall_b0_h1_vert_spv_len},
        {shadow_wall_b1_h1_vert_spv, shadow_wall_b1_h1_vert_spv_len},
    };
    for (size_t i = 0; i < 4; ++i) {
        resources.roofVertex[i] = createShader(resources, roofShaders[i].data, roofShaders[i].size);
        resources.wallVertex[i] = createShader(resources, wallShaders[i].data, wallShaders[i].size);
    }
    resources.maskFragment = createShader(resources, shadow_mask_frag_spv, shadow_mask_frag_spv_len);
    resources.compositeVertex = createShader(resources, shadow_composite_vert_spv, shadow_composite_vert_spv_len);
    resources.compositeFragment = createShader(resources, shadow_composite_frag_spv, shadow_composite_frag_spv_len);
    for (size_t i = 0; i < 4; ++i) {
        if (!resources.roofVertex[i] || !resources.wallVertex[i]) {
            return fail(resources, "failed to create a Vulkan shadow shader module");
        }
    }
    if (!resources.maskFragment || !resources.compositeVertex || !resources.compositeFragment) {
        return fail(resources, "failed to create a Vulkan shadow shader module");
    }
    return true;
}

VkFormat attributeFormat(mln_plugin_attribute_type type) {
    switch (type) {
        case MLN_PLUGIN_ATTRIBUTE_INT16_X2:
            return VK_FORMAT_R16G16_SINT;
        case MLN_PLUGIN_ATTRIBUTE_UINT16_X2:
            return VK_FORMAT_R16G16_UINT;
        case MLN_PLUGIN_ATTRIBUTE_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case MLN_PLUGIN_ATTRIBUTE_FLOAT_X2:
            return VK_FORMAT_R32G32_SFLOAT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

VkPipeline createMaskPipeline(VulkanResources& resources, const mln_plugin_draw_packet_v1& packet) {
    VkVertexInputBindingDescription bindings[7]{};
    VkVertexInputAttributeDescription attributes[7]{};
    uint32_t bindingCount = 0;
    uint32_t attributeCount = 0;
    const auto add =
        [&](uint32_t binding, uint32_t location, const mln_plugin_buffer_binding_v1& value, VkVertexInputRate rate) {
            const auto format = attributeFormat(value.type);
            if (!value.buffer || !value.stride || format == VK_FORMAT_UNDEFINED) return;
            bindings[bindingCount++] = {binding, value.stride, rate};
            attributes[attributeCount++] = {location, binding, format, 0};
        };

    if (packet.kind == MLN_PLUGIN_DRAW_PACKET_INSTANCED_WALLS) {
        add(0, 0, packet.wall_vertex, VK_VERTEX_INPUT_RATE_VERTEX);
        add(1, 1, packet.position, VK_VERTEX_INPUT_RATE_INSTANCE);
        add(2, 2, packet.decimals_edge, VK_VERTEX_INPUT_RATE_INSTANCE);
        add(3, 3, packet.position, VK_VERTEX_INPUT_RATE_INSTANCE);
        add(4, 4, packet.decimals_edge, VK_VERTEX_INPUT_RATE_INSTANCE);
        if (packet.base_is_attribute) add(5, 5, packet.base, VK_VERTEX_INPUT_RATE_INSTANCE);
        if (packet.height_is_attribute) add(6, 6, packet.height, VK_VERTEX_INPUT_RATE_INSTANCE);
    } else {
        add(0, 0, packet.position, VK_VERTEX_INPUT_RATE_VERTEX);
        add(1, 1, packet.decimals_edge, VK_VERTEX_INPUT_RATE_VERTEX);
        if (packet.base_is_attribute) add(2, 2, packet.base, VK_VERTEX_INPUT_RATE_VERTEX);
        if (packet.height_is_attribute) add(3, 3, packet.height, VK_VERTEX_INPUT_RATE_VERTEX);
    }

    const VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                           nullptr,
                                                           0,
                                                           bindingCount,
                                                           bindings,
                                                           attributeCount,
                                                           attributes};
    const VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                          nullptr,
                                                          0,
                                                          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                          VK_FALSE};
    const VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    const VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                        nullptr,
                                                        0,
                                                        VK_FALSE,
                                                        VK_FALSE,
                                                        VK_POLYGON_MODE_FILL,
                                                        VK_CULL_MODE_NONE,
                                                        VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                        VK_FALSE,
                                                        0.0f,
                                                        0.0f,
                                                        0.0f,
                                                        1.0f};
    const VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                           nullptr,
                                                           0,
                                                           VK_SAMPLE_COUNT_1_BIT,
                                                           VK_FALSE,
                                                           0.0f,
                                                           nullptr,
                                                           VK_FALSE,
                                                           VK_FALSE};
    const VkPipelineColorBlendAttachmentState blend{VK_TRUE,
                                                    VK_BLEND_FACTOR_ONE,
                                                    VK_BLEND_FACTOR_ONE,
                                                    VK_BLEND_OP_MAX,
                                                    VK_BLEND_FACTOR_ONE,
                                                    VK_BLEND_FACTOR_ONE,
                                                    VK_BLEND_OP_MAX,
                                                    VK_COLOR_COMPONENT_R_BIT};
    const VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         nullptr,
                                                         0,
                                                         VK_FALSE,
                                                         VK_LOGIC_OP_COPY,
                                                         1,
                                                         &blend,
                                                         {0, 0, 0, 0}};
    constexpr VkDynamicState dynamicStates[2]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dynamicStates};
    const VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         nullptr,
         0,
         VK_SHADER_STAGE_VERTEX_BIT,
         packet.kind == MLN_PLUGIN_DRAW_PACKET_INSTANCED_WALLS
             ? resources.wallVertex[(packet.base_is_attribute ? 1u : 0u) | (packet.height_is_attribute ? 2u : 0u)]
             : resources.roofVertex[(packet.base_is_attribute ? 1u : 0u) | (packet.height_is_attribute ? 2u : 0u)],
         "main",
         nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         nullptr,
         0,
         VK_SHADER_STAGE_FRAGMENT_BIT,
         resources.maskFragment,
         "main",
         nullptr},
    };
    const VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                            nullptr,
                                            0,
                                            2,
                                            stages,
                                            &vertexInput,
                                            &assembly,
                                            nullptr,
                                            &viewport,
                                            &raster,
                                            &multisample,
                                            nullptr,
                                            &blendState,
                                            &dynamic,
                                            resources.maskPipelineLayout,
                                            resources.maskRenderPass,
                                            0,
                                            VK_NULL_HANDLE,
                                            -1};
    VkPipeline pipeline = VK_NULL_HANDLE;
    return resources.fn.createGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) ==
                   VK_SUCCESS
               ? pipeline
               : VK_NULL_HANDLE;
}

VkPipeline maskPipeline(VulkanResources& resources, const mln_plugin_draw_packet_v1& packet) {
    for (size_t i = 0; i < resources.maskPipelineCount; ++i) {
        if (resources.maskPipelines[i].matches(packet)) return resources.maskPipelines[i].pipeline;
    }
    if (resources.maskPipelineCount == MaxMaskPipelines) return VK_NULL_HANDLE;
    PipelineEntry entry{};
    entry.kind = packet.kind;
    entry.positionType = packet.position.type;
    entry.decimalsType = packet.decimals_edge.type;
    entry.baseType = packet.base.type;
    entry.heightType = packet.height.type;
    entry.wallStride = packet.wall_vertex.stride;
    entry.positionStride = packet.position.stride;
    entry.decimalsStride = packet.decimals_edge.stride;
    entry.baseStride = packet.base.stride;
    entry.heightStride = packet.height.stride;
    entry.baseAttribute = packet.base_is_attribute;
    entry.heightAttribute = packet.height_is_attribute;
    entry.pipeline = createMaskPipeline(resources, packet);
    resources.maskPipelines[resources.maskPipelineCount++] = entry;
    return entry.pipeline;
}

VkPipeline createCompositePipeline(VulkanResources& resources, VkRenderPass renderPass) {
    const VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         nullptr,
         0,
         VK_SHADER_STAGE_VERTEX_BIT,
         resources.compositeVertex,
         "main",
         nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         nullptr,
         0,
         VK_SHADER_STAGE_FRAGMENT_BIT,
         resources.compositeFragment,
         "main",
         nullptr},
    };
    const VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    const VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                          nullptr,
                                                          0,
                                                          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                          VK_FALSE};
    const VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    const VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                        nullptr,
                                                        0,
                                                        VK_FALSE,
                                                        VK_FALSE,
                                                        VK_POLYGON_MODE_FILL,
                                                        VK_CULL_MODE_NONE,
                                                        VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                        VK_FALSE,
                                                        0,
                                                        0,
                                                        0,
                                                        1};
    const VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE};
    const VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    const VkPipelineColorBlendAttachmentState blend{
        VK_TRUE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    const VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         nullptr,
                                                         0,
                                                         VK_FALSE,
                                                         VK_LOGIC_OP_COPY,
                                                         1,
                                                         &blend,
                                                         {0, 0, 0, 0}};
    constexpr VkDynamicState dynamicStates[2]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dynamicStates};
    const VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                            nullptr,
                                            0,
                                            2,
                                            stages,
                                            &vertexInput,
                                            &assembly,
                                            nullptr,
                                            &viewport,
                                            &raster,
                                            &multisample,
                                            &depthStencil,
                                            &blendState,
                                            &dynamic,
                                            resources.compositePipelineLayout,
                                            renderPass,
                                            0,
                                            VK_NULL_HANDLE,
                                            -1};
    VkPipeline pipeline = VK_NULL_HANDLE;
    return resources.fn.createGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) ==
                   VK_SUCCESS
               ? pipeline
               : VK_NULL_HANDLE;
}

bool ensureResources(ShadowInstance* instance, const mln_plugin_frame_context_v1* frame) {
    const auto& backend = *frame->backend;
    auto* existing = static_cast<VulkanResources*>(instance->vk.resources);
    const auto device = deviceHandle(backend.device);
    if (existing && existing->device == device && existing->width == frame->width &&
        existing->height == frame->height) {
        return true;
    }
    if (existing && existing->fn.deviceWaitIdle) {
        existing->fn.deviceWaitIdle(existing->device);
    }
    destroyResources(existing);
    instance->vk.resources = nullptr;

    auto* resources = static_cast<VulkanResources*>(calloc(1, sizeof(VulkanResources)));
    if (!resources) {
        shadowLog(instance, 3, "failed to allocate Vulkan shadow resources");
        return false;
    }
    resources->physicalDevice = physicalDeviceHandle(backend.physical_device);
    resources->device = device;
    resources->width = frame->width > 0 ? frame->width : 1u;
    resources->height = frame->height > 0 ? frame->height : 1u;
    if (!backend.get_proc_address) {
        shadowLog(instance, 3, "Vulkan procedure resolver is unavailable");
        destroyResources(resources);
        return false;
    }
    if (!loadFunctions(resources->fn, backend)) {
        shadowLog(instance, 3, "failed to resolve a required Vulkan procedure");
        destroyResources(resources);
        return false;
    }
    if (!createResources(*resources)) {
        shadowLog(instance,
                  3,
                  resources->failure ? resources->failure : "failed to create Vulkan fill-extrusion shadow resources");
        destroyResources(resources);
        return false;
    }
    instance->vk.resources = resources;
    return true;
}

void bindPacket(VulkanResources& resources, VkCommandBuffer commandBuffer, const mln_plugin_draw_packet_v1& packet) {
    VkBuffer buffers[7]{};
    VkDeviceSize offsets[7]{};
    uint32_t count = 0;
    const auto add = [&](const mln_plugin_buffer_binding_v1& binding, uint64_t extraOffset = 0) {
        buffers[count] = bufferHandle(binding.buffer);
        offsets[count] = binding.offset + extraOffset;
        ++count;
    };
    if (packet.kind == MLN_PLUGIN_DRAW_PACKET_INSTANCED_WALLS) {
        add(packet.wall_vertex);
        add(packet.position);
        add(packet.decimals_edge);
        add(packet.position, packet.position.stride);
        add(packet.decimals_edge, packet.decimals_edge.stride);
        if (packet.base_is_attribute) add(packet.base);
        if (packet.height_is_attribute) add(packet.height);
    } else {
        add(packet.position);
        add(packet.decimals_edge);
        if (packet.base_is_attribute) add(packet.base);
        if (packet.height_is_attribute) add(packet.height);
    }
    resources.fn.cmdBindVertexBuffers(commandBuffer, 0, count, buffers, offsets);
}

ShadowPush pushFor(const mln_plugin_draw_packet_v1& packet) {
    ShadowPush push{};
    memcpy(push.matrix, packet.tile_matrix, sizeof(push.matrix));
    push.constantBase = packet.constant_base;
    push.constantHeight = packet.constant_height;
    push.baseInterpolation = packet.base_interpolation;
    push.heightInterpolation = packet.height_interpolation;
    push.heightFactor = packet.height_factor;
    push.alpha = 0.35f * packet.layer_opacity;
    push.baseAttribute = packet.base_is_attribute;
    push.heightAttribute = packet.height_is_attribute;
    return push;
}

} // namespace

mln_plugin_status shadowVulkanPrepare(ShadowInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !frame || !frame->backend || !frame->backend->command_buffer ||
        !ensureResources(instance, frame)) {
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    auto& resources = *static_cast<VulkanResources*>(instance->vk.resources);
    const auto commandBuffer = commandBufferHandle(frame->backend->command_buffer);
    const VkClearValue clearValue{{{0.0f, 0.0f, 0.0f, 0.0f}}};
    const VkRenderPassBeginInfo beginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                          nullptr,
                                          resources.maskRenderPass,
                                          resources.maskFramebuffer,
                                          {{0, 0}, {resources.width, resources.height}},
                                          1,
                                          &clearValue};
    resources.fn.cmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(resources.width), static_cast<float>(resources.height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, {resources.width, resources.height}};
    resources.fn.cmdSetViewport(commandBuffer, 0, 1, &viewport);
    resources.fn.cmdSetScissor(commandBuffer, 0, 1, &scissor);

    for (size_t i = 0; i < frame->fill_extrusion_packet_count; ++i) {
        const auto& packet = frame->fill_extrusion_packets[i];
        if (!packet.index_buffer || !packet.position.buffer || !packet.decimals_edge.buffer ||
            (packet.kind == MLN_PLUGIN_DRAW_PACKET_INSTANCED_WALLS && !packet.wall_vertex.buffer)) {
            continue;
        }
        const auto pipeline = maskPipeline(resources, packet);
        if (!pipeline) {
            resources.fn.cmdEndRenderPass(commandBuffer);
            shadowLog(instance, 3, "failed to create Vulkan shadow mask pipeline");
            return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
        }
        resources.fn.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        bindPacket(resources, commandBuffer, packet);
        resources.fn.cmdBindIndexBuffer(
            commandBuffer, bufferHandle(packet.index_buffer), packet.index_offset, VK_INDEX_TYPE_UINT16);
        const auto push = pushFor(packet);
        resources.fn.cmdPushConstants(
            commandBuffer, resources.maskPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
        resources.fn.cmdDrawIndexed(commandBuffer,
                                    packet.index_count,
                                    packet.instance_count > 0 ? packet.instance_count : 1u,
                                    0,
                                    packet.base_vertex,
                                    0);
    }
    resources.fn.cmdEndRenderPass(commandBuffer);
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status shadowVulkanRender(ShadowInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !frame || !frame->backend || !frame->backend->command_buffer || !frame->backend->render_pass ||
        !ensureResources(instance, frame)) {
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    auto& resources = *static_cast<VulkanResources*>(instance->vk.resources);
    const auto commandBuffer = commandBufferHandle(frame->backend->command_buffer);
    const auto renderPass = renderPassHandle(frame->backend->render_pass);
    if (!resources.compositePipeline || resources.compositeRenderPass != renderPass) {
        if (resources.compositePipeline) {
            resources.fn.destroyPipeline(resources.device, resources.compositePipeline, nullptr);
        }
        resources.compositePipeline = createCompositePipeline(resources, renderPass);
        resources.compositeRenderPass = renderPass;
    }
    if (!resources.compositePipeline) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;

    const VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(frame->width), static_cast<float>(frame->height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, {frame->width, frame->height}};
    resources.fn.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.compositePipeline);
    resources.fn.cmdSetViewport(commandBuffer, 0, 1, &viewport);
    resources.fn.cmdSetScissor(commandBuffer, 0, 1, &scissor);
    resources.fn.cmdBindDescriptorSets(commandBuffer,
                                       VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       resources.compositePipelineLayout,
                                       0,
                                       1,
                                       &resources.compositeSet,
                                       0,
                                       nullptr);
    float alpha = 0.35f;
    if (frame->fill_extrusion_packet_count) {
        alpha *= frame->fill_extrusion_packets[0].layer_opacity;
    }
    resources.fn.cmdPushConstants(
        commandBuffer, resources.compositePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(alpha), &alpha);
    resources.fn.cmdDraw(commandBuffer, 3, 1, 0, 0);
    return MLN_PLUGIN_STATUS_OK;
}

void shadowVulkanContextLost(ShadowInstance* instance) {
    if (!instance) return;
    auto* resources = static_cast<VulkanResources*>(instance->vk.resources);
    if (resources && resources->fn.deviceWaitIdle) {
        resources->fn.deviceWaitIdle(resources->device);
    }
    destroyResources(resources);
    instance->vk.resources = nullptr;
}

void shadowVulkanDestroy(ShadowInstance* instance) {
    shadowVulkanContextLost(instance);
}
