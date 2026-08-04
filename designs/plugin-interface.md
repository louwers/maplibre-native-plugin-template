# MapLibre Native existing-layer plugin interface

Status: Android v1 implementation. The ABI is platform-neutral; the iOS package and Metal adapter are deferred.

## Design goal

A plugin can add typed constant properties and rendering behavior to an existing style-layer type without adding plugin-specific code to MapLibre Native. Core knows how to register generic descriptors, store generic property values, schedule lifecycle callbacks, and expose short-lived geometry packets. It does not know that a shadow plugin exists, what a shadow means, or how a shadow is drawn.

Adding another plugin requires publishing a new plugin library. A core change is needed only when a future plugin category needs a genuinely new generic host capability, such as a geometry packet type not represented by v1.

## Boundary

The public boundary is the pure-C header `mbgl/plugin/plugin_api.h`. Android publishes it in the STL-free `org.maplibre.gl:android-plugin-api` Prefab package; the renderer AARs continue to export the four C symbols from `libmaplibre.so`. An Android plugin receives the registration function address from `MapLibrePluginRegistry` and invokes that typed C function pointer from JNI. It never links to MapLibre C++ classes, STL types, generated style-layer code, or renderer vtables.

Core owns:

- descriptor validation and process-wide registration;
- generic constant paint-property parsing, storage, cloning, lookup, serialization, observer notification, and repaint scheduling;
- deterministic callback ordering and per-render-layer plugin instances;
- backend context setup, graphics-state invalidation, failure isolation, and short-lived draw packets;
- adapters that describe existing fill-extrusion buffers without copying or changing them.

The plugin owns:

- the property names and their interpretation;
- all shaders, pipelines, framebuffer/mask resources, blend choices, and shadow mathematics;
- Android convenience APIs and JNI registration;
- resource recreation after resize/context loss and resource destruction.

## Registration and compatibility

`mln_plugin_descriptor_v1` is size-versioned and contains a stable plugin ID/version, required host ABI interval, and one or more existing-layer extensions. Each extension names a target layer type, priority, supported backend mask, typed properties, and lifecycle callbacks.

The v1 value types are boolean, float, float2, RGBA color, and length-aware UTF-8 string. Only constant paint properties are accepted. Expressions, transitions, coercion, data-driven plugin properties, and layout properties are rejected.

`mln_plugin_register_v1` copies strings, property metadata, and defaults into core-owned storage. The native library continues to own callback code and must stay loaded for the process lifetime. Unloading and unregistering are intentionally unsupported.

Registration is thread-safe. It must happen before a style that uses a plugin property is parsed. Identical repeated registration succeeds as `ALREADY_REGISTERED`. Core rejects malformed descriptors, incompatible ABI ranges, conflicting IDs, duplicate properties, and conflicting layer extensions with a diagnostic copied into the caller's buffer.

## Style behavior

Generated layer setters first handle their built-in properties, then use the registry by `(layer type, property name)`. Unknown properties remain style errors when the plugin has not been registered.

Plugin values are stored in a generic bag in immutable layer implementation state. An absent value resolves to the registered default but is not serialized. An explicitly set value, including `false`, is retained and serialized. Clone and runtime mutation copy or replace the bag exactly. Runtime mutations use the normal layer observer path and therefore trigger repaint through the existing Android `Layer.setProperties(...)` API.

## Threads, lifecycle, and ownership

Registration may occur on any thread. Applications should perform it on startup before constructing or loading a dependent style.

`create_instance`, `prepare_frame`, `render_before_layer`, `context_lost`, and `destroy_instance` are called on the render thread. There is one instance per matching render layer. The plugin must not call MapLibre recursively or block the render thread.

Property snapshots, backend handles, command buffers, draw packets, and buffer handles are borrowed and valid only until the callback returns. The plugin may copy scalar values but must never retain a host graphics handle. Plugin-created GPU resources are owned by the plugin instance.

Callbacks return a status. On failure, core logs once, destroys and disables only that layer's plugin instance, invalidates cached graphics state as applicable, and continues rendering the base layer.

## Render stages and packets

After drawable upload and before the main render pass, core calls `prepare_frame`. Immediately before the target layer group in its normal pass, core calls `render_before_layer`, placing a plugin composite beneath the unmodified base layer.

Fill-extrusion packets expose the already-uploaded index and semantic vertex buffers, tile matrix, evaluated constant values, interpolation factors, extrusion height conversion factor, and layer opacity. OpenGL exposes the ordinary triangle mesh. Vulkan exposes roof triangles and instanced walls separately. Plugins must use packet-declared offsets, strides, and attribute types and must not mutate buffer contents.

OpenGL callbacks may use GLES calls directly. Vulkan callbacks record only into the supplied command buffer/render-pass context and resolve Vulkan procedures through the host function. After either callback, the host invalidates cached graphics state and rebinds MapLibre's global uniform descriptors when a render pass is active; this is required because a plugin's Vulkan pipeline layout can invalidate descriptor sets bound by the host. No Vulkan C++ types cross the ABI.

## Android packaging

MapLibre's canonical, OpenGL, and Vulkan AARs export the registration symbols and depend on `org.maplibre.gl:android-plugin-api` at the same version. The API AAR contains the Java registry contract and an STL-free Prefab module with the C header. A plugin uses the exact API snapshot as `compileOnly`; a separate non-native Gradle configuration transforms only the exact renderer AAR's `classes.jar` for typed Java helpers. Neither dependency appears in the plugin POM. The application chooses one renderer:

```kotlin
openglImplementation("org.maplibre.gl:android-sdk-opengl:<exact-version>")
vulkanImplementation("org.maplibre.gl:android-sdk-vulkan:<exact-version>")
implementation("org.maplibre.plugins:fill-extrusion-shadows:<exact-version>")
```

MapLibre uses `c++_static` and keeps its C++ runtime private. Plugins may independently use C++, STL, exceptions, and their own `c++_static` runtime. They must not pass C++/STL objects, exceptions, RTTI identities, or C++ allocation ownership across the ABI; an allocation is destroyed by the DSO that created it. The C API artifact itself is compiled with `ANDROID_STL=none`.

The plugin wrapper first calls `MapLibrePluginRegistry.ensureMapLibreLoaded()`, then loads its own native library, obtains the process-lifetime v1 registration function address from the registry, and invokes it through JNI. This avoids a native link between two independently static-linked C++ DSOs. The registry inspection API reports process-wide IDs and status without depending on any plugin artifact.

## Worked example: fill-extrusion shadows

The plugin registers:

- ID `org.maplibre.fill-extrusion-shadows`;
- target layer `fill-extrusion`;
- boolean paint property `fill-extrusion-shadow`, default `false`;
- OpenGL and Vulkan rendering callbacks.

When the property is enabled, the plugin projects upper extrusion vertices toward fixed v1 offset `[-0.5, 0.5]` with height scale `0.38`. It renders black with alpha `0.35 * fill-extrusion-opacity`, then leaves the original building layer untouched to render on top. OpenGL uses stencil and Vulkan uses a `MAX` blend into the single-channel mask; both produce a union mask so overlapping projected geometry cannot darken twice. All constants and shader code reside in this repository.

Future properties (`-x`, `-y`, `-h-scale`, `-intensity`, and `-blur`) become additional descriptor entries and plugin snapshot reads. They do not require generated core layer properties or a new ABI.

## iOS and Metal roadmap

The C descriptor and registry are already portable. A later iOS package will load a plugin framework, call the same registration function, and provide Swift/Objective-C result wrappers. A Metal backend adapter will populate an equivalent short-lived backend context and draw packets. The removed experimental `MLNPluginLayer` API is not part of this design and will not be kept as a parallel ABI.

## Versioning rules

All public structs begin with `struct_size`; callbacks read only fields available in the negotiated ABI. Additive fields append to structs. Semantic or ownership changes require a new ABI version and registration symbol. Plugin artifact versions and host artifact versions remain independent, while CI compiles each plugin against the exact host snapshot used by the validation app.
