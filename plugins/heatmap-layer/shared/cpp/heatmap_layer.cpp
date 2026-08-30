#include "heatmap_layer.hpp"
#include "heatmap_shader_sources.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

#ifndef MLN_HEATMAP_PLUGIN_VERSION
#define MLN_HEATMAP_PLUGIN_VERSION "0.1.0-local"
#endif

namespace {

using namespace maplibre::plugins::heatmap;

constexpr uint32_t positionAttribute = 0;
constexpr uint32_t cornerAttribute = 1;
constexpr uint32_t weightAttribute = 2;
constexpr uint32_t radiusAttribute = 3;
constexpr uint32_t vertexStream = 0;

constexpr mln_plugin_string str(const char *value, size_t size) {
  return {value, size};
}

template <size_t N> constexpr mln_plugin_string str(const char (&value)[N]) {
  return str(value, N - 1);
}

constexpr mln_plugin_value makeFloat(float value) {
  mln_plugin_value result{};
  result.struct_size = sizeof(result);
  result.type = MLN_PLUGIN_VALUE_FLOAT;
  result.data.float_value = value;
  return result;
}

constexpr mln_plugin_value makeColorRamp(const char *value, size_t size) {
  mln_plugin_value result{};
  result.struct_size = sizeof(result);
  result.type = MLN_PLUGIN_VALUE_COLOR_RAMP;
  result.data.color_ramp_json = str(value, size);
  return result;
}

template <size_t N>
constexpr mln_plugin_value makeColorRamp(const char (&value)[N]) {
  return makeColorRamp(value, N - 1);
}

mln_plugin_property_descriptor_v1 floatProperty(mln_plugin_string name,
                                                float defaultValue,
                                                float minimum, float maximum,
                                                bool hasMaximum) {
  mln_plugin_property_descriptor_v1 property{};
  property.struct_size = sizeof(property);
  property.name = name;
  property.type = MLN_PLUGIN_VALUE_FLOAT;
  property.scope = MLN_PLUGIN_PROPERTY_PAINT;
  property.default_value = makeFloat(defaultValue);
  property.supports_expressions = 1;
  property.supports_transitions = 0;
  property.has_minimum = 1;
  property.has_maximum = hasMaximum;
  property.minimum = minimum;
  property.maximum = maximum;
  return property;
}

constexpr char defaultColorRamp[] =
    R"JSON(["interpolate",["linear"],["heatmap-density"],0,"rgba(0, 0, 255, 0)",0.1,"royalblue",0.3,"cyan",0.5,"lime",0.7,"yellow",1,"red"])JSON";

const std::array<mln_plugin_property_descriptor_v1, 5> properties = [] {
  std::array<mln_plugin_property_descriptor_v1, 5> result{};
  result[0].struct_size = sizeof(mln_plugin_property_descriptor_v1);
  result[0].name = str("heatmap-color");
  result[0].type = MLN_PLUGIN_VALUE_COLOR_RAMP;
  result[0].scope = MLN_PLUGIN_PROPERTY_PAINT;
  result[0].default_value = makeColorRamp(defaultColorRamp);
  result[0].supports_expressions = 1;
  result[1] = floatProperty(str("heatmap-intensity"), 1.0f, 0.0f, 0.0f, false);
  result[2] = floatProperty(str("heatmap-opacity"), 1.0f, 0.0f, 1.0f, true);
  result[3] = floatProperty(str("heatmap-radius"), 30.0f, 1.0f, 0.0f, false);
  result[4] = floatProperty(str("heatmap-weight"), 1.0f, 0.0f, 0.0f, false);
  return result;
}();

struct Vertex {
  int16_t position[2];
  int16_t corner[2];
  float weight;
  float radius;
};

static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, corner) == 4);
static_assert(offsetof(Vertex, weight) == 8);
static_assert(offsetof(Vertex, radius) == 12);
static_assert(sizeof(Vertex) == 16);

struct Layout {
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
  std::vector<mln_plugin_segment_v1> segments;
  std::array<mln_plugin_vertex_stream_v1, 1> streams{};
  std::array<mln_plugin_attribute_binding_v1, 4> attributes{};
  std::array<mln_plugin_drawable_descriptor_v1, 1> drawables{};
  uint32_t extent = 8192;
};

const mln_plugin_value *property(const mln_plugin_feature_v1 &feature,
                                 const char *name) {
  const auto length = std::strlen(name);
  for (size_t i = 0; i < feature.evaluated_property_count; ++i) {
    const auto &candidate = feature.evaluated_properties[i];
    if (candidate.name.data && candidate.name.size == length &&
        std::memcmp(candidate.name.data, name, length) == 0) {
      return &candidate.value;
    }
  }
  return nullptr;
}

const mln_plugin_value *property(const mln_plugin_uniform_context_v1 &context,
                                 const char *name) {
  const auto length = std::strlen(name);
  for (size_t i = 0; i < context.property_count; ++i) {
    const auto &candidate = context.properties[i];
    if (candidate.name.data && candidate.name.size == length &&
        std::memcmp(candidate.name.data, name, length) == 0) {
      return &candidate.value;
    }
  }
  return nullptr;
}

float number(const mln_plugin_feature_v1 &feature, const char *name,
             float fallback) {
  const auto *value = property(feature, name);
  return value && value->type == MLN_PLUGIN_VALUE_FLOAT &&
                 std::isfinite(value->data.float_value)
             ? value->data.float_value
             : fallback;
}

float number(const mln_plugin_uniform_context_v1 &context, const char *name,
             float fallback) {
  const auto *value = property(context, name);
  return value && value->type == MLN_PLUGIN_VALUE_FLOAT &&
                 std::isfinite(value->data.float_value)
             ? value->data.float_value
             : fallback;
}

void startSegment(Layout &layout) {
  mln_plugin_segment_v1 segment{};
  segment.struct_size = sizeof(segment);
  segment.vertex_offset = static_cast<uint32_t>(layout.vertices.size());
  segment.index_offset = static_cast<uint32_t>(layout.indices.size());
  layout.segments.push_back(segment);
}

mln_plugin_status createLayout(const mln_plugin_layout_context_v1 *context,
                               void **instance) {
  if (!context || context->struct_size < sizeof(*context) || !instance) {
    return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto *layout = new (std::nothrow) Layout();
  if (!layout)
    return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
  layout->extent = context->extent;
  startSegment(*layout);
  *instance = layout;
  return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status layoutFeature(void *instance,
                                const mln_plugin_feature_v1 *feature) {
  if (!instance || !feature || feature->struct_size < sizeof(*feature) ||
      !feature->points ||
      (feature->geometry_type != MLN_PLUGIN_GEOMETRY_POINT &&
       feature->geometry_type != MLN_PLUGIN_GEOMETRY_LINESTRING &&
       feature->geometry_type != MLN_PLUGIN_GEOMETRY_POLYGON)) {
    return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto &layout = *static_cast<Layout *>(instance);
  const float weight = std::max(0.0f, number(*feature, "heatmap-weight", 1.0f));
  const float radius =
      std::max(1.0f, number(*feature, "heatmap-radius", 30.0f));

  for (size_t pointIndex = 0; pointIndex < feature->point_count; ++pointIndex) {
    const auto point = feature->points[pointIndex];
    if (point.x < 0 || static_cast<uint32_t>(point.x) >= layout.extent ||
        point.y < 0 || static_cast<uint32_t>(point.y) >= layout.extent)
      continue;
    if (layout.segments.back().vertex_length >
        std::numeric_limits<uint16_t>::max() - 4u) {
      startSegment(layout);
    }
    auto &segment = layout.segments.back();
    const uint16_t base = static_cast<uint16_t>(segment.vertex_length);
    constexpr int16_t corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (const auto &corner : corners) {
      layout.vertices.push_back(
          {{point.x, point.y}, {corner[0], corner[1]}, weight, radius});
    }
    const uint16_t quad[] = {
        base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
        base, static_cast<uint16_t>(base + 3), static_cast<uint16_t>(base + 2)};
    layout.indices.insert(layout.indices.end(), std::begin(quad),
                          std::end(quad));
    segment.vertex_length += 4;
    segment.index_length += 6;
    segment.feature_index = feature->feature_index;
  }
  return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status finishLayout(void *instance, mln_plugin_bucket_v1 *output) {
  if (!instance || !output || output->struct_size < sizeof(*output)) {
    return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  auto &layout = *static_cast<Layout *>(instance);
  layout.segments.erase(std::remove_if(layout.segments.begin(),
                                       layout.segments.end(),
                                       [](const auto &segment) {
                                         return segment.index_length == 0;
                                       }),
                        layout.segments.end());
  layout.streams[0] = {
      sizeof(mln_plugin_vertex_stream_v1),
      vertexStream,
      reinterpret_cast<const uint8_t *>(layout.vertices.data()),
      layout.vertices.size() * sizeof(Vertex),
      static_cast<uint32_t>(layout.vertices.size()),
      sizeof(Vertex)};
  layout.attributes = {{
      {sizeof(mln_plugin_attribute_binding_v1), positionAttribute, vertexStream,
       offsetof(Vertex, position), MLN_PLUGIN_VERTEX_INT16_X2},
      {sizeof(mln_plugin_attribute_binding_v1), cornerAttribute, vertexStream,
       offsetof(Vertex, corner), MLN_PLUGIN_VERTEX_INT16_X2},
      {sizeof(mln_plugin_attribute_binding_v1), weightAttribute, vertexStream,
       offsetof(Vertex, weight), MLN_PLUGIN_VERTEX_FLOAT},
      {sizeof(mln_plugin_attribute_binding_v1), radiusAttribute, vertexStream,
       offsetof(Vertex, radius), MLN_PLUGIN_VERTEX_FLOAT},
  }};
  auto &drawable = layout.drawables[0];
  drawable.struct_size = sizeof(drawable);
  drawable.drawable_key = 1;
  drawable.shader_id = str("heatmap-kernel");
  drawable.draw_mode = MLN_PLUGIN_DRAW_MODE_TRIANGLES;
  drawable.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
  drawable.depth_mode = MLN_PLUGIN_DEPTH_DISABLED;
  drawable.blend_mode = MLN_PLUGIN_BLEND_ADDITIVE;
  drawable.attributes = layout.attributes.data();
  drawable.attribute_count = layout.attributes.size();
  drawable.segments = layout.segments.data();
  drawable.segment_count = layout.segments.size();

  output->vertex_streams =
      layout.vertices.empty() ? nullptr : layout.streams.data();
  output->vertex_stream_count =
      layout.vertices.empty() ? 0 : layout.streams.size();
  output->indices = layout.indices.empty() ? nullptr : layout.indices.data();
  output->index_count = layout.indices.size();
  output->drawables =
      layout.indices.empty() ? nullptr : layout.drawables.data();
  output->drawable_count = layout.indices.empty() ? 0 : layout.drawables.size();
  output->query_radius = 0.0f;
  return MLN_PLUGIN_STATUS_OK;
}

void destroyLayout(void *instance) { delete static_cast<Layout *>(instance); }

struct alignas(16) KernelUBO {
  float matrix[16];
  float pixelsToTileUnits;
  float intensity;
  float padding[2];
};

struct alignas(16) CompositeUBO {
  float matrix[16];
  float opacity;
  float padding[3];
};

static_assert(sizeof(KernelUBO) == 80);
static_assert(sizeof(CompositeUBO) == 80);

mln_plugin_status updateUniform(const mln_plugin_uniform_context_v1 *context,
                                uint32_t uniformID, uint8_t *output,
                                size_t outputSize) {
  if (!context || context->struct_size < sizeof(*context) || !output ||
      uniformID != 0) {
    return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (context->pass_id == 1 && outputSize == sizeof(KernelUBO)) {
    KernelUBO value{};
    std::copy_n(context->tile_matrix, 16, value.matrix);
    value.pixelsToTileUnits = context->pixels_to_tile_units;
    value.intensity = number(*context, "heatmap-intensity", 1.0f);
    std::memcpy(output, &value, sizeof(value));
    return MLN_PLUGIN_STATUS_OK;
  }
  if (context->pass_id == 2 && outputSize == sizeof(CompositeUBO)) {
    CompositeUBO value{};
    std::copy_n(context->viewport_matrix, 16, value.matrix);
    value.opacity = number(*context, "heatmap-opacity", 1.0f);
    std::memcpy(output, &value, sizeof(value));
    return MLN_PLUGIN_STATUS_OK;
  }
  return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
}

const std::array<mln_plugin_shader_attribute_v1, 4> kernelAttributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), positionAttribute, 0,
     str("a_position"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), cornerAttribute, 1,
     str("a_corner"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), weightAttribute, 2,
     str("a_weight"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), radiusAttribute, 3,
     str("a_radius"), MLN_PLUGIN_VERTEX_FLOAT},
}};

const std::array<mln_plugin_shader_attribute_v1, 1> compositeAttributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), positionAttribute, 0,
     str("a_position"), MLN_PLUGIN_VERTEX_INT16_X2},
}};

const std::array<mln_plugin_uniform_block_descriptor_v1, 1> kernelUniforms = {{
    {sizeof(mln_plugin_uniform_block_descriptor_v1), 0, str("HeatmapKernelUBO"),
     sizeof(KernelUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX | MLN_PLUGIN_SHADER_STAGE_FRAGMENT,
     MLN_PLUGIN_UNIFORM_SCOPE_DRAWABLE},
}};

const std::array<mln_plugin_uniform_block_descriptor_v1, 1> compositeUniforms =
    {{
        {sizeof(mln_plugin_uniform_block_descriptor_v1), 0,
         str("HeatmapCompositeUBO"), sizeof(CompositeUBO),
         MLN_PLUGIN_SHADER_STAGE_VERTEX | MLN_PLUGIN_SHADER_STAGE_FRAGMENT,
         MLN_PLUGIN_UNIFORM_SCOPE_LAYER},
    }};

const std::array<mln_plugin_shader_texture_v1, 2> compositeTextures = {{
    {sizeof(mln_plugin_shader_texture_v1), 0, 0, str("u_density")},
    {sizeof(mln_plugin_shader_texture_v1), 1, 1, str("u_color_ramp")},
}};

const std::array<mln_plugin_shader_source_v1, 3> kernelSources = {{
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_OPENGL,
     str(shaders::glKernelVertex,
         std::char_traits<char>::length(shaders::glKernelVertex)),
     str(shaders::glKernelFragment,
         std::char_traits<char>::length(shaders::glKernelFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_VULKAN,
     str(shaders::vulkanKernelVertex,
         std::char_traits<char>::length(shaders::vulkanKernelVertex)),
     str(shaders::vulkanKernelFragment,
         std::char_traits<char>::length(shaders::vulkanKernelFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_METAL,
     str(shaders::metalKernel,
         std::char_traits<char>::length(shaders::metalKernel)),
     {},
     str("vertexMain"),
     str("fragmentMain")},
}};

const std::array<mln_plugin_shader_source_v1, 3> compositeSources = {{
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_OPENGL,
     str(shaders::glCompositeVertex,
         std::char_traits<char>::length(shaders::glCompositeVertex)),
     str(shaders::glCompositeFragment,
         std::char_traits<char>::length(shaders::glCompositeFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_VULKAN,
     str(shaders::vulkanCompositeVertex,
         std::char_traits<char>::length(shaders::vulkanCompositeVertex)),
     str(shaders::vulkanCompositeFragment,
         std::char_traits<char>::length(shaders::vulkanCompositeFragment)),
     {},
     {}},
    {sizeof(mln_plugin_shader_source_v1),
     MLN_PLUGIN_BACKEND_METAL,
     str(shaders::metalComposite,
         std::char_traits<char>::length(shaders::metalComposite)),
     {},
     str("vertexMain"),
     str("fragmentMain")},
}};

const std::array<mln_plugin_shader_descriptor_v1, 2> shaderDescriptors = {{
    {sizeof(mln_plugin_shader_descriptor_v1), str("heatmap-kernel"),
     kernelSources.data(), kernelSources.size(), kernelAttributes.data(),
     kernelAttributes.size(), kernelUniforms.data(), kernelUniforms.size(),
     nullptr, 0},
    {sizeof(mln_plugin_shader_descriptor_v1), str("heatmap-composite"),
     compositeSources.data(), compositeSources.size(),
     compositeAttributes.data(), compositeAttributes.size(),
     compositeUniforms.data(), compositeUniforms.size(),
     compositeTextures.data(), compositeTextures.size()},
}};

const std::array<mln_plugin_render_target_descriptor_v1, 1> renderTargets = {{
    {sizeof(mln_plugin_render_target_descriptor_v1),
     1,
     MLN_PLUGIN_RENDER_TARGET_VIEWPORT,
     MLN_PLUGIN_RENDER_TARGET_RGBA16F,
     MLN_PLUGIN_RENDER_TARGET_PER_LAYER,
     0.5f,
     0.5f,
     {0.0f, 0.0f, 0.0f, 1.0f}},
}};

const std::array<mln_plugin_texture_binding_v1, 2> compositeBindings = {{
    {sizeof(mln_plugin_texture_binding_v1),
     0,
     MLN_PLUGIN_TEXTURE_SOURCE_RENDER_TARGET,
     1,
     {},
     MLN_PLUGIN_TEXTURE_FILTER_LINEAR,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP},
    {sizeof(mln_plugin_texture_binding_v1), 1,
     MLN_PLUGIN_TEXTURE_SOURCE_COLOR_RAMP_PROPERTY, 0, str("heatmap-color"),
     MLN_PLUGIN_TEXTURE_FILTER_LINEAR, MLN_PLUGIN_TEXTURE_WRAP_CLAMP,
     MLN_PLUGIN_TEXTURE_WRAP_CLAMP},
}};

const std::array<mln_plugin_render_pass_descriptor_v1, 2> renderPasses = {{
    {sizeof(mln_plugin_render_pass_descriptor_v1), 1, str("heatmap-kernel"),
     MLN_PLUGIN_GRAPH_GEOMETRY_PLUGIN_BUCKET, 1,
     MLN_PLUGIN_RENDER_STAGE_PREPARE, MLN_PLUGIN_DRAW_MODE_TRIANGLES,
     MLN_PLUGIN_DEPTH_DISABLED, MLN_PLUGIN_BLEND_ADDITIVE, 0, 0,
     MLN_PLUGIN_TILE_PROJECTION_MAP, nullptr, 0},
    {sizeof(mln_plugin_render_pass_descriptor_v1), 2, str("heatmap-composite"),
     MLN_PLUGIN_GRAPH_GEOMETRY_VIEWPORT_QUAD, 0,
     MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT, MLN_PLUGIN_DRAW_MODE_TRIANGLES,
     MLN_PLUGIN_DEPTH_DISABLED, MLN_PLUGIN_BLEND_PREMULTIPLIED_ALPHA, 0, 0,
     MLN_PLUGIN_TILE_PROJECTION_MAP, compositeBindings.data(),
     compositeBindings.size()},
}};

const mln_plugin_render_graph_v1 renderGraph = {
    sizeof(mln_plugin_render_graph_v1),
    renderTargets.data(),
    renderTargets.size(),
    renderPasses.data(),
    renderPasses.size(),
};

const mln_plugin_layer_type_v1 layerType = [] {
  mln_plugin_layer_type_v1 value{};
  value.struct_size = sizeof(value);
  value.layer_type = str("org.maplibre.heatmap");
  value.backend_mask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN |
                       MLN_PLUGIN_BACKEND_METAL;
  value.render_stage = MLN_PLUGIN_RENDER_STAGE_TRANSLUCENT;
  value.properties = properties.data();
  value.property_count = properties.size();
  value.source_kind = MLN_PLUGIN_SOURCE_GEOMETRY;
  value.geometry_type_mask = MLN_PLUGIN_GEOMETRY_POINT |
                             MLN_PLUGIN_GEOMETRY_LINESTRING |
                             MLN_PLUGIN_GEOMETRY_POLYGON;
  value.shaders = shaderDescriptors.data();
  value.shader_count = shaderDescriptors.size();
  value.create_layout = createLayout;
  value.layout_feature = layoutFeature;
  value.finish_layout = finishLayout;
  value.destroy_layout = destroyLayout;
  value.render_graph = &renderGraph;
  value.update_uniform_block = updateUniform;
  value.participates_in_3d_pass = 1;
  return value;
}();

const mln_plugin_descriptor_v1 descriptor = {
    sizeof(mln_plugin_descriptor_v1),
    MLN_PLUGIN_ABI_VERSION_1,
    str("org.maplibre.heatmap"),
    str(MLN_HEATMAP_PLUGIN_VERSION, sizeof(MLN_HEATMAP_PLUGIN_VERSION) - 1),
    MLN_PLUGIN_ABI_VERSION_1,
    MLN_PLUGIN_ABI_VERSION_1,
    nullptr,
    0,
    &layerType,
    1,
};

} // namespace

extern "C" mln_plugin_status
mln_heatmap_layer_register(mln_plugin_register_function_v1 registerPlugin,
                           char *errorMessage, size_t errorMessageCapacity) {
  if (!registerPlugin)
    return MLN_PLUGIN_STATUS_NOT_FOUND;
  return registerPlugin(&descriptor, errorMessage, errorMessageCapacity);
}
