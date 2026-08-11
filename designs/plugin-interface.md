# MapLibre Native plugin interface

Status: Android OpenGL/Vulkan and iOS Metal v1 implementations.

## Design goal

A plugin can add typed constant properties and rendering behavior to an existing style-layer type, or register a new source-less style-layer type, without adding plugin-specific code to MapLibre Native. Core knows how to register generic descriptors, store generic property values, schedule lifecycle callbacks, expose short-lived geometry packets, and load plugin resources. It does not know that a shadow or GLTF plugin exists.

Adding another plugin requires publishing a new plugin library. A core change is needed only when a future plugin category needs a genuinely new generic host capability, such as a geometry packet type not represented by v1.

## Boundary

The public boundary is the pure-C header `mbgl/plugin/plugin_api.h`. The plugin keeps a matching copy of that versioned header so it can compile independently, while each Android renderer AAR exports the registration symbols from `libmaplibre.so`. The Android wrapper locates `MapLibrePluginRegistry` reflectively, obtains the registration function address, and invokes that typed C function pointer from JNI; this avoids a transitive renderer or companion-API dependency in the plugin POM. On iOS, `MLNPluginAPI.h` exposes the same types and registration symbol through the MapLibre framework. Neither platform boundary exposes MapLibre C++ classes, STL types, generated style-layer code, or renderer vtables.

Core owns:

- descriptor validation and process-wide registration;
- generic constant paint/layout property parsing, storage, cloning, lookup, serialization, observer notification, and repaint scheduling;
- source-less plugin layer creation through the common `LayerManager` fallback, without changing generated/platform factory maps;
- camera snapshots and resource requests routed through MapLibre's `FileSource`;
- deterministic callback ordering and per-render-layer plugin instances;
- backend context setup, graphics-state invalidation, failure isolation, and short-lived draw packets;
- adapters that describe existing fill-extrusion buffers without copying or changing them.

The plugin owns:

- the property names and their interpretation;
- all shaders, pipelines, framebuffer/mask resources, blend choices, and shadow mathematics;
- Android convenience APIs and JNI registration;
- resource recreation after resize/context loss and resource destruction.

## Registration and compatibility

`mln_plugin_descriptor_v1` contains a stable plugin ID/version, required host ABI interval, and one or more existing-layer extensions and/or new layer-type declarations. An extension names an existing target type and callback priority. A layer-type declaration names the new style type, render stage, 3D behavior, properties, supported backends, and lifecycle callbacks.

The v1 value types are boolean, float, float2, RGBA color, and length-aware UTF-8 string. Existing-layer extensions accept constant paint properties. New plugin layers accept constant paint and layout properties. Expressions, transitions, coercion, and data-driven plugin properties are rejected.

`mln_plugin_register_v1` copies strings, property metadata, and defaults into core-owned storage. The native library continues to own callback code and must stay loaded for the process lifetime. Unloading and unregistering are intentionally unsupported.

Registration is thread-safe. It must happen before a style that uses a plugin property or type is parsed. Identical repeated registration succeeds as `ALREADY_REGISTERED`. Core rejects malformed descriptors, incompatible ABI ranges, conflicting IDs or layer types, duplicate properties, and conflicting extensions with a diagnostic copied into the caller's buffer.

## Style behavior

Generated layer setters first handle their built-in properties, then use the registry by `(layer type, property name)`. Unknown properties remain style errors when the plugin has not been registered.

Plugin values are stored in a generic bag in immutable layer implementation state. An absent value resolves to the registered default but is not serialized. An explicitly set value, including `false`, is retained and serialized. Clone and runtime mutation copy or replace the bag exactly. Runtime mutations use the normal layer observer path and therefore trigger repaint through the existing Android `Layer.setProperties(...)` API.

## Threads, lifecycle, and ownership

Registration may occur on any thread. Applications should perform it on startup before constructing or loading a dependent style.

`create_instance`, `prepare_frame`, the layer render callback, `context_lost`, and `destroy_instance` are called on the render thread. There is one instance per matching render layer. File-loader completion is delivered on the requesting render run loop. Response bytes are borrowed only during that callback; a plugin copies anything it retains. Host request IDs can be cancelled, and `request_repaint` marshals invalidation through the renderer observer.

Property snapshots, backend handles, command buffers, draw packets, and buffer handles are borrowed and valid only until the callback returns. The plugin may copy scalar values but must never retain a host graphics handle. Plugin-created GPU resources are owned by the plugin instance.

Callbacks return a status. On failure, core logs once, destroys and disables only that layer's plugin instance, invalidates cached graphics state as applicable, and continues rendering the base layer.

## Render stages and packets

After drawable upload and before the main render pass, core calls `prepare_frame`. Immediately before the target layer group in its normal pass, core calls `render_before_layer`, placing a plugin composite beneath the unmodified base layer.

A new source-less plugin layer gets an ordinary ordered layer group but no generated bucket or drawable. Its descriptor-selected callback runs at that group's exact position in the 3D, opaque, or translucent pass. The frame includes viewport/camera values, normalized-Mercator projection matrices, pixel ratio, Vulkan surface pre-rotation, and backend handles.

Fill-extrusion packets expose the already-uploaded index and semantic vertex buffers, tile matrix, evaluated constant values, interpolation factors, extrusion height conversion factor, and layer opacity. OpenGL exposes the ordinary triangle mesh. Vulkan exposes roof triangles and instanced walls separately. Plugins must use packet-declared offsets, strides, and attribute types and must not mutate buffer contents.

OpenGL callbacks may use GLES calls directly. Vulkan callbacks record only into the supplied command buffer/render-pass context and resolve Vulkan procedures through the host function. Metal callbacks receive borrowed Objective-C objects as opaque C pointers: device, queue, command buffer, and active render encoder, plus attachment formats and sample count. After a callback, the host invalidates backend state and rebinds its global resources; the Metal render pass explicitly resets its cached pipeline, buffer, depth/stencil, cull, and scissor state. No Vulkan C++ or Metal C++ types cross the ABI.

## Android packaging

MapLibre's canonical, OpenGL, and Vulkan AARs export the registration symbols and provide `MapLibrePluginRegistry`. The plugin vendors the ABI v1 C header and compiles its typed Java property helper against MapLibre's public Java API with `compileOnly`. Its wrapper discovers the registry reflectively, so neither MapLibre nor a companion API artifact appears in the plugin POM. The application chooses one renderer:

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

The `gltf-layer` plugin registers source-less type `gltf` for Android OpenGL/Vulkan and iOS Metal. Its constant layout properties are `model-uri`, `model-position`, `model-altitude`, `model-heading`, and `model-scale`; `model-opacity` is a paint property. A model URI is requested through the host API, so MapLibre's cache, resource transform, online/offline policy, and error path remain authoritative.

The shared implementation parses GLB bytes with pinned TinyGLTF v2.9.7, flattens scene/node transforms into a backend-neutral vertex/index model, and maps glTF's Y-up meter coordinates into normalized Web Mercator at the requested longitude/latitude. Each backend owns only its vertex/index buffers, shaders, and pipeline. V1 renders static triangle meshes, base-color factors, alpha blending, and double-sided materials. Texture upload, animation, skinning, morph targets, compressed geometry, and terrain anchoring are explicitly deferred.

## iOS and Metal

The static `FillExtrusionShadows` framework provides an Objective-C registration wrapper and calls the same C descriptor code as Android. The plugin is registered before a dependent `MLNStyle` is parsed. MapLibre passes the active Metal command buffer, encoder, and existing fill-extrusion buffers only for the callback duration. The prepare callback builds the union mask in a private `R8Unorm` render target; the before-layer callback samples it in one full-screen composite. The plugin retains only its own pipelines, mask texture, and depth/stencil state. The removed experimental `MLNPluginLayer` API is not part of this design and is not kept as a parallel ABI.

The Bazel sample uses a local MapLibre module override so it compiles against the exact host ABI implementation. Swift Package Manager distribution is layered on the same framework target; it does not introduce another plugin interface.

## ABI v1 evolution

This API has not been published, so the implementation remains ABI v1 and no parallel v2 surface is introduced. Public structs retain `struct_size` for defensive validation and future additive growth. Plugin artifact versions and host artifact versions remain independent, while CI compiles each plugin against the exact host snapshot used by the validation app.
