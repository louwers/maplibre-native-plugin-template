# MapLibre Native plugin interface

Status: Android OpenGL/Vulkan and iOS Metal v1 implementations.

## Design goal

A plugin can add typed properties and rendering behavior to an existing style-layer type, or register a new source-bound style-layer type, without adding plugin-specific code to MapLibre Native. Core knows how to register generic descriptors, store and evaluate generic property values, participate in source layout, own plugin drawables, register backend shaders, expose short-lived existing-layer geometry packets, and load plugin resources. It does not know that a shadow, GLTF, or rectangle plugin exists.

Adding another plugin requires publishing a new plugin library. A core change is needed only when a future plugin category needs a genuinely new generic host capability, such as a geometry packet type not represented by v1.

## Boundary

The public boundary is the pure-C header `mln/plugin/plugin_api.h`. Android plugins compile it from MapLibre's renderer-independent `android-plugin-api` Prefab artifact. Bazel plugins depend on MapLibre's `//:plugin-api` target, while an iOS framework exposes the same declarations through `MLNPluginAPI.h`. The Android wrapper locates `MapLibrePluginRegistry` reflectively, obtains the registration function address, and invokes that typed C function pointer from JNI. Neither platform boundary exposes MapLibre C++ classes, STL types, generated style-layer code, or renderer vtables.

Core owns:

- descriptor validation and process-wide registration;
- generic typed paint/layout property parsing, expression evaluation, constraint validation, storage, cloning, lookup, serialization, observer notification, and repaint scheduling;
- source-bound plugin layer creation through the common `LayerManager` fallback, without changing generated/platform factory maps;
- geometry- and RasterDEM-tile scheduling, per-feature/camera expression evaluation, bucket validation, feature indexing, and drawable updates/removal;
- host-owned render targets, pass ordering, DEM textures, tile masks, shader uniform buffers, and texture bindings for declarative render graphs;
- shader registration for OpenGL, Vulkan, and Metal and resource requests routed through MapLibre's `FileSource`;
- deterministic callback ordering and per-render-layer plugin instances;
- backend context setup, graphics-state invalidation, failure isolation, and short-lived draw packets;
- adapters that describe existing fill-extrusion buffers without copying or changing them.

The plugin owns:

- the property names and their interpretation;
- extension shaders/pipelines/framebuffer resources and custom-layer shader source, declared uniform/texture resources, vertex layouts, bucket data, render graphs, and draw state;
- Android convenience APIs and JNI registration;
- resource recreation after resize/context loss and resource destruction.

## Registration and compatibility

`mln_plugin_descriptor_v1` contains a stable plugin ID/version, required host ABI interval, and one or more existing-layer extensions and/or new layer-type declarations. An extension names an existing target type and callback priority. A layer-type declaration names the new style type, required source kind, render stage, 3D behavior, properties, supported backends, shaders, and either geometry-layout callbacks or a RasterDEM render graph.

The v1 value types are boolean, float, float2, RGBA color, length-aware UTF-8 string, float array, and color array. Descriptors can allow expressions and scalar authoring for array values, constrain numeric ranges/array lengths, and enumerate valid strings. Core evaluates camera expressions on the render thread and data-driven expressions during geometry layout. Transitions and coercion remain outside the current host implementation.

`mln_plugin_register_v1` copies strings, property metadata, and defaults into core-owned storage. The native library continues to own callback code and must stay loaded for the process lifetime. Unloading and unregistering are intentionally unsupported.

Registration is thread-safe. It must happen before a style that uses a plugin property or type is parsed. Identical repeated registration succeeds as `ALREADY_REGISTERED`. Core rejects malformed descriptors, incompatible ABI ranges, conflicting IDs or layer types, duplicate properties, and conflicting extensions with a diagnostic copied into the caller's buffer.

## Style behavior

Generated layer setters first handle their built-in properties, then use the registry by `(layer type, property name)`. Unknown properties remain style errors when the plugin has not been registered.

Plugin values are stored in a generic bag in immutable layer implementation state. An absent value resolves to the registered default but is not serialized. An explicitly set value, including `false`, is retained and serialized. Clone and runtime mutation copy or replace the bag exactly. Runtime mutations use the normal layer observer path and therefore trigger repaint through the existing Android `Layer.setProperties(...)` API.

## Threads, lifecycle, and ownership

Registration may occur on any thread. Applications should perform it on startup before constructing or loading a dependent style.

Existing-layer extension callbacks (`create_instance`, `prepare_frame`, `render_before_layer`, `context_lost`, and `destroy_instance`) are called on the render thread. There is one instance per matching render layer. Custom layer types do not receive backend contexts or issue render commands. Their `create_layout`, `layout_feature`, `finish_layout`, and `destroy_layout` callbacks run during geometry-tile layout and return copied CPU bucket data. Layout resource requests are completed through a dedicated FileSource run loop before CPU layout proceeds. Response bytes are borrowed only during the resource callback; a plugin copies anything it retains.

Property snapshots, backend handles, command buffers, draw packets, and buffer handles are borrowed and valid only until the callback returns. The plugin may copy scalar values but must never retain a host graphics handle. Plugin-created GPU resources are owned by the plugin instance.

Callbacks return a status. On failure, core logs once, destroys and disables only that layer's plugin instance, invalidates cached graphics state as applicable, and continues rendering the base layer.

## Render stages and packets

After drawable upload and before the main render pass, core calls `prepare_frame`. Immediately before the target layer group in its normal pass, core calls `render_before_layer`, placing a plugin composite beneath the unmodified base layer.

A geometry plugin layer names a GeoJSON/vector source. MapLibre schedules its visible tiles, passes supported source features through CPU layout, validates and copies returned vertex/index/segment data, uploads it through ordinary buckets, and creates/removes drawables in the layer's ordered tile group. Each drawable names a registered shader and declares draw, depth, blend, stencil, and cull state. Direct custom-layer render callbacks are intentionally not part of the API.

Every plugin shader explicitly declares its attributes, uniform blocks, textures, stages, and resource scopes. Core assigns backend bindings and injects numeric binding macros into GLSL/MSL. The plugin fills host-owned uniform blocks through `update_uniform_block`; the callback receives tile matrices, camera state, evaluated property snapshots, and RasterDEM metadata. There is no implicit plugin uniform layout.

A RasterDEM plugin layer instead supplies a declarative, topologically ordered render graph. A pass selects host full-tile or tile-mask geometry, a registered shader, an optional host render target, texture inputs, and ordinary draw/depth/blend/stencil/cull state. Core owns DEM upload, offscreen textures, render-target layer groups, masked final drawables, resize/context replacement, and drawable removal. Render-target outputs may feed later passes without exposing backend commands or C++ renderer objects to the plugin.

Fill-extrusion packets expose the already-uploaded index and semantic vertex buffers, tile matrix, evaluated constant values, interpolation factors, extrusion height conversion factor, and layer opacity. OpenGL exposes the ordinary triangle mesh. Vulkan exposes roof triangles and instanced walls separately. Plugins must use packet-declared offsets, strides, and attribute types and must not mutate buffer contents.

OpenGL callbacks may use GLES calls directly. Vulkan callbacks record only into the supplied command buffer/render-pass context and resolve Vulkan procedures through the host function. Metal callbacks receive borrowed Objective-C objects as opaque C pointers: device, queue, command buffer, and active render encoder, plus attachment formats and sample count. After a callback, the host invalidates backend state and rebinds its global resources; the Metal render pass explicitly resets its cached pipeline, buffer, depth/stencil, cull, and scissor state. No Vulkan C++ or Metal C++ types cross the ABI.

## Android packaging

MapLibre's canonical, OpenGL, and Vulkan AARs export the registration symbols and provide `MapLibrePluginRegistry`. The renderer-independent `android-plugin-api` AAR supplies only the Prefab C header/anchor and is safe as the plugin's compile dependency. Java convenience wrappers compile against MapLibre's public Java API with `compileOnly`. The application chooses one renderer:

```kotlin
openglImplementation("org.maplibre.gl:android-sdk-opengl:<exact-version>")
vulkanImplementation("org.maplibre.gl:android-sdk-vulkan:<exact-version>")
implementation("org.maplibre.plugins:fill-extrusion-shadows:<exact-version>")
```

MapLibre uses `c++_static` and keeps its C++ runtime private. Plugins may independently use C++, STL, exceptions, and their own `c++_static` runtime. They must not pass C++/STL objects, exceptions, RTTI identities, or C++ allocation ownership across the ABI; an allocation is destroyed by the DSO that created it. Any separately published C API artifact is compiled with `ANDROID_STL=none`.

The plugin wrapper first calls `MapLibrePluginRegistry.ensureMapLibreLoaded()`, then loads its own native library, obtains the process-lifetime v1 registration function address from the registry, and invokes it through JNI. This avoids a native link between two independently static-linked C++ DSOs. The registry inspection API reports process-wide IDs and status without depending on any plugin artifact.

## Worked example: fill-extrusion shadows

The plugin registers:

- ID `org.maplibre.fill-extrusion-shadows`;
- target layer `fill-extrusion`;
- boolean paint property `fill-extrusion-shadow`, default `false`;
- OpenGL, Vulkan, and Metal rendering callbacks.

When the property is enabled, the plugin projects upper extrusion vertices toward fixed v1 offset `[-0.5, 0.5]` with height scale `0.38`. It renders black with alpha `0.35 * fill-extrusion-opacity`, then leaves the original building layer untouched to render on top. Every backend first rasterizes coverage into a cleared, single-channel union mask and composites that mask exactly once. OpenGL uses stencil, while Vulkan and Metal use `MAX` blending; repeated coverage from overlapping triangles therefore remains `1.0` instead of accumulating opacity. Metal consumes its separately exposed roof packet and decodes constant, scalar, or float2-interpolated heights. All constants and shader code reside in this repository.

Future properties (`-x`, `-y`, `-h-scale`, `-intensity`, and `-blur`) become additional descriptor entries and plugin snapshot reads. They do not require generated core layer properties or a new ABI.

## Worked example: GLTF layer

The `gltf-layer` plugin registers source-bound type `gltf` for Android OpenGL/Vulkan and iOS Metal. GeoJSON or vector-tile point features supply model anchors. Its layout properties are `model-uri`, `model-altitude`, `model-heading`, and `model-scale`; `model-opacity` is a paint property. A model URI is requested through the layout host API, so MapLibre's cache, resource transform, online/offline policy, and error path remain authoritative.

The shared implementation parses GLB bytes with pinned TinyGLTF v2.9.7, flattens scene/node transforms, converts glTF's Y-up meter coordinates into tile x/y plus meter z at every point feature, and returns segmented 16-bit index buffers. MapLibre owns the vertex/index buffers, shaders, pipelines, drawable lifetime, ordering, visibility, and depth state. V1 renders static indexed triangle meshes and base-color factors. Texture upload, animation, skinning, morph targets, compressed geometry, and terrain anchoring are explicitly deferred.

## Worked example: hillshade layer

The `hillshade-layer` plugin registers source-bound RasterDEM type `org.maplibre.hillshade` while the built-in `hillshade` implementation remains available. Its first graph pass samples the host DEM texture over full-tile geometry and writes encoded derivatives into a host RGBA8 tile-sized render target. Its second pass samples that target over the host's DEM tile-mask geometry and blends into the map in the translucent/3D ordering used by built-in hillshade.

The plugin declares the built-in hillshade paint surface, including scalar-or-array illumination directions/altitudes and highlight/shadow colors, enum-constrained method/anchor values, numeric limits, and camera expressions. Its uniform callback produces the same prepare, tile, and evaluated data used by the built-in OpenGL, Vulkan, and Metal shaders. Plugin-owned render tests contain the complete built-in hillshade fixture set with only the style-layer type changed; offline DEM/raster data and platform-specific expectations live with the plugin.

## iOS and Metal

The static `FillExtrusionShadows` framework provides an Objective-C registration wrapper and calls the same C descriptor code as Android. The plugin is registered before a dependent `MLNStyle` is parsed. MapLibre passes the active Metal command buffer, encoder, and existing fill-extrusion buffers only for the callback duration. The prepare callback builds the union mask in a private `R8Unorm` render target; the before-layer callback samples it in one full-screen composite. The plugin retains only its own pipelines, mask texture, and depth/stencil state. The removed experimental `MLNPluginLayer` API is not part of this design and is not kept as a parallel ABI.

The Bazel sample uses a local MapLibre module override so it compiles against the exact host ABI implementation. Swift Package Manager distribution is layered on the same framework target; it does not introduce another plugin interface.

## ABI v1 evolution

This API has not been published, so the implementation remains ABI v1 and no parallel v2 surface is introduced. Public structs retain `struct_size` for defensive validation and future additive growth. Plugin artifact versions and host artifact versions remain independent, while CI compiles each plugin against the exact host snapshot used by the validation app.
