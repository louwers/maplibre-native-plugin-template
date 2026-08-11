#include <vulkan/vulkan.h>

#include "gltf_layer.hpp"

#include "shaders/gltf.vert.spv.inc"
#include "shaders/gltf.frag.spv.inc"

#include <cstddef>
#include <cstring>

namespace {

struct Functions {
    PFN_vkCreateBuffer createBuffer = nullptr;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements = nullptr;
    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    PFN_vkBindBufferMemory bindBufferMemory = nullptr;
    PFN_vkMapMemory mapMemory = nullptr;
    PFN_vkUnmapMemory unmapMemory = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkCreateShaderModule createShaderModule = nullptr;
    PFN_vkDestroyShaderModule destroyShaderModule = nullptr;
    PFN_vkCreatePipelineLayout createPipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout destroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines createGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    PFN_vkCmdBindPipeline cmdBindPipeline = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer = nullptr;
    PFN_vkCmdPushConstants cmdPushConstants = nullptr;
    PFN_vkCmdSetViewport cmdSetViewport = nullptr;
    PFN_vkCmdSetScissor cmdSetScissor = nullptr;
    PFN_vkCmdDrawIndexed cmdDrawIndexed = nullptr;
};

struct VulkanResources {
    Functions fn;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    VkShaderModule vertexShader = VK_NULL_HANDLE;
    VkShaderModule fragmentShader = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint64_t generation = 0;
};

template <class T>
bool load(T& function, const mln_plugin_backend_context_v1* backend, const char* name) {
    function = reinterpret_cast<T>(backend->get_proc_address(backend->resolver_context, name));
    return function != nullptr;
}

bool loadFunctions(Functions& fn, const mln_plugin_backend_context_v1* backend) {
    return load(fn.createBuffer, backend, "vkCreateBuffer") &&
           load(fn.destroyBuffer, backend, "vkDestroyBuffer") &&
           load(fn.getBufferMemoryRequirements, backend, "vkGetBufferMemoryRequirements") &&
           load(fn.allocateMemory, backend, "vkAllocateMemory") && load(fn.freeMemory, backend, "vkFreeMemory") &&
           load(fn.bindBufferMemory, backend, "vkBindBufferMemory") && load(fn.mapMemory, backend, "vkMapMemory") &&
           load(fn.unmapMemory, backend, "vkUnmapMemory") &&
           load(fn.getPhysicalDeviceMemoryProperties, backend, "vkGetPhysicalDeviceMemoryProperties") &&
           load(fn.createShaderModule, backend, "vkCreateShaderModule") &&
           load(fn.destroyShaderModule, backend, "vkDestroyShaderModule") &&
           load(fn.createPipelineLayout, backend, "vkCreatePipelineLayout") &&
           load(fn.destroyPipelineLayout, backend, "vkDestroyPipelineLayout") &&
           load(fn.createGraphicsPipelines, backend, "vkCreateGraphicsPipelines") &&
           load(fn.destroyPipeline, backend, "vkDestroyPipeline") &&
           load(fn.cmdBindPipeline, backend, "vkCmdBindPipeline") &&
           load(fn.cmdBindVertexBuffers, backend, "vkCmdBindVertexBuffers") &&
           load(fn.cmdBindIndexBuffer, backend, "vkCmdBindIndexBuffer") &&
           load(fn.cmdPushConstants, backend, "vkCmdPushConstants") &&
           load(fn.cmdSetViewport, backend, "vkCmdSetViewport") &&
           load(fn.cmdSetScissor, backend, "vkCmdSetScissor") &&
           load(fn.cmdDrawIndexed, backend, "vkCmdDrawIndexed");
}

void destroyBuffer(VulkanResources& resources, VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer) resources.fn.destroyBuffer(resources.device, buffer, nullptr);
    if (memory) resources.fn.freeMemory(resources.device, memory, nullptr);
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
}

void destroy(VulkanResources* resources) {
    if (!resources) return;
    if (resources->pipeline) resources->fn.destroyPipeline(resources->device, resources->pipeline, nullptr);
    if (resources->pipelineLayout) {
        resources->fn.destroyPipelineLayout(resources->device, resources->pipelineLayout, nullptr);
    }
    if (resources->vertexShader) resources->fn.destroyShaderModule(resources->device, resources->vertexShader, nullptr);
    if (resources->fragmentShader) {
        resources->fn.destroyShaderModule(resources->device, resources->fragmentShader, nullptr);
    }
    destroyBuffer(*resources, resources->vertexBuffer, resources->vertexMemory);
    destroyBuffer(*resources, resources->indexBuffer, resources->indexMemory);
    delete resources;
}

uint32_t memoryType(VulkanResources& resources, uint32_t bits) {
    VkPhysicalDeviceMemoryProperties properties{};
    resources.fn.getPhysicalDeviceMemoryProperties(resources.physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        const auto flags = properties.memoryTypes[i].propertyFlags;
        if ((bits & (1u << i)) && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool createBuffer(VulkanResources& resources,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  const void* bytes,
                  VkBuffer& buffer,
                  VkDeviceMemory& memory) {
    const VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  nullptr,
                                  0,
                                  size,
                                  usage,
                                  VK_SHARING_MODE_EXCLUSIVE,
                                  0,
                                  nullptr};
    if (resources.fn.createBuffer(resources.device, &info, nullptr, &buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements requirements{};
    resources.fn.getBufferMemoryRequirements(resources.device, buffer, &requirements);
    const uint32_t type = memoryType(resources, requirements.memoryTypeBits);
    if (type == UINT32_MAX) return false;
    const VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size, type};
    if (resources.fn.allocateMemory(resources.device, &allocation, nullptr, &memory) != VK_SUCCESS) return false;
    if (resources.fn.bindBufferMemory(resources.device, buffer, memory, 0) != VK_SUCCESS) return false;
    void* mapped = nullptr;
    if (resources.fn.mapMemory(resources.device, memory, 0, size, 0, &mapped) != VK_SUCCESS) return false;
    std::memcpy(mapped, bytes, static_cast<size_t>(size));
    resources.fn.unmapMemory(resources.device, memory);
    return true;
}

VkShaderModule shader(VulkanResources& resources, const unsigned char* bytes, size_t size) {
    const VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                        nullptr,
                                        0,
                                        size,
                                        reinterpret_cast<const uint32_t*>(bytes)};
    VkShaderModule result = VK_NULL_HANDLE;
    return resources.fn.createShaderModule(resources.device, &info, nullptr, &result) == VK_SUCCESS ? result
                                                                                                     : VK_NULL_HANDLE;
}

bool ensureBase(VulkanResources& resources, const mln_plugin_backend_context_v1* backend) {
    if (resources.device) return true;
    resources.device = reinterpret_cast<VkDevice>(backend->device);
    resources.physicalDevice = reinterpret_cast<VkPhysicalDevice>(backend->physical_device);
    if (!resources.device || !resources.physicalDevice || !backend->get_proc_address ||
        !loadFunctions(resources.fn, backend)) {
        return false;
    }
    resources.vertexShader = shader(resources, gltf_vert_spv, gltf_vert_spv_len);
    resources.fragmentShader = shader(resources, gltf_frag_spv, gltf_frag_spv_len);
    const VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 80};
    const VkPipelineLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushRange};
    return resources.vertexShader && resources.fragmentShader &&
           resources.fn.createPipelineLayout(resources.device, &layoutInfo, nullptr, &resources.pipelineLayout) ==
               VK_SUCCESS;
}

bool upload(VulkanResources& resources, const GltfLayerInstance& instance) {
    destroyBuffer(resources, resources.vertexBuffer, resources.vertexMemory);
    destroyBuffer(resources, resources.indexBuffer, resources.indexMemory);
    if (!createBuffer(resources,
                      instance.model->vertices.size() * sizeof(GltfVertex),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      instance.model->vertices.data(),
                      resources.vertexBuffer,
                      resources.vertexMemory) ||
        !createBuffer(resources,
                      instance.model->indices.size() * sizeof(uint32_t),
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      instance.model->indices.data(),
                      resources.indexBuffer,
                      resources.indexMemory)) {
        return false;
    }
    resources.generation = instance.modelGeneration;
    return true;
}

bool createPipeline(VulkanResources& resources, VkRenderPass renderPass) {
    if (resources.pipeline && resources.renderPass == renderPass) return true;
    if (resources.pipeline) resources.fn.destroyPipeline(resources.device, resources.pipeline, nullptr);
    resources.pipeline = VK_NULL_HANDLE;
    resources.renderPass = renderPass;

    const VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         nullptr,
         0,
         VK_SHADER_STAGE_VERTEX_BIT,
         resources.vertexShader,
         "main",
         nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         nullptr,
         0,
         VK_SHADER_STAGE_FRAGMENT_BIT,
         resources.fragmentShader,
         "main",
         nullptr}};
    const VkVertexInputBindingDescription binding{0, sizeof(GltfVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attributes[3]{{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GltfVertex, position)},
                                                          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GltfVertex, normal)},
                                                          {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GltfVertex, color)}};
    const VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                           nullptr,
                                                           0,
                                                           1,
                                                           &binding,
                                                           3,
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
    const VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                      nullptr,
                                                      0,
                                                      VK_TRUE,
                                                      VK_TRUE,
                                                      VK_COMPARE_OP_LESS_OR_EQUAL,
                                                      VK_FALSE,
                                                      VK_FALSE,
                                                      {},
                                                      {},
                                                      0.0f,
                                                      1.0f};
    const VkPipelineColorBlendAttachmentState attachment{VK_TRUE,
                                                         VK_BLEND_FACTOR_ONE,
                                                         VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                         VK_BLEND_OP_ADD,
                                                         VK_BLEND_FACTOR_ONE,
                                                         VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                         VK_BLEND_OP_ADD,
                                                         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    const VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                    nullptr,
                                                    0,
                                                    VK_FALSE,
                                                    VK_LOGIC_OP_COPY,
                                                    1,
                                                    &attachment,
                                                    {0, 0, 0, 0}};
    constexpr VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, states};
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
                                            &depth,
                                            &blend,
                                            &dynamic,
                                            resources.pipelineLayout,
                                            renderPass,
                                            0,
                                            VK_NULL_HANDLE,
                                            -1};
    return resources.fn.createGraphicsPipelines(
               resources.device, VK_NULL_HANDLE, 1, &info, nullptr, &resources.pipeline) == VK_SUCCESS;
}

struct alignas(16) PushConstants {
    float matrix[16];
    float opacity;
    float padding[3];
};

} // namespace

mln_plugin_status gltfRenderVulkan(GltfLayerInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !instance->model || !frame || !frame->backend || !frame->backend->command_buffer ||
        !frame->backend->render_pass) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto* resources = static_cast<VulkanResources*>(instance->vulkan);
    if (!resources) {
        resources = new VulkanResources();
        instance->vulkan = resources;
    }
    if (!ensureBase(*resources, frame->backend) ||
        (resources->generation != instance->modelGeneration && !upload(*resources, *instance)) ||
        !createPipeline(*resources, reinterpret_cast<VkRenderPass>(frame->backend->render_pass))) {
        gltfLog(instance, 3, "Unable to create GLTF Vulkan resources");
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    PushConstants push{};
    if (!gltfModelMatrix(frame, push.matrix, push.opacity) || push.opacity <= 0.0f) return MLN_PLUGIN_STATUS_OK;
    const auto commandBuffer = reinterpret_cast<VkCommandBuffer>(frame->backend->command_buffer);
    const VkViewport viewport{0.0f,
                              0.0f,
                              static_cast<float>(frame->width),
                              static_cast<float>(frame->height),
                              0.0f,
                              1.0f};
    const VkRect2D scissor{{0, 0}, {frame->width, frame->height}};
    const VkDeviceSize offset = 0;
    resources->fn.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources->pipeline);
    resources->fn.cmdSetViewport(commandBuffer, 0, 1, &viewport);
    resources->fn.cmdSetScissor(commandBuffer, 0, 1, &scissor);
    resources->fn.cmdBindVertexBuffers(commandBuffer, 0, 1, &resources->vertexBuffer, &offset);
    resources->fn.cmdBindIndexBuffer(commandBuffer, resources->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    resources->fn.cmdPushConstants(commandBuffer,
                                   resources->pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0,
                                   sizeof(push),
                                   &push);
    resources->fn.cmdDrawIndexed(
        commandBuffer, static_cast<uint32_t>(instance->model->indices.size()), 1, 0, 0, 0);
    return MLN_PLUGIN_STATUS_OK;
}

void gltfDestroyVulkan(GltfLayerInstance* instance) {
    if (!instance) return;
    destroy(static_cast<VulkanResources*>(instance->vulkan));
    instance->vulkan = nullptr;
}
