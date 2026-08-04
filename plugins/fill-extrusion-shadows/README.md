# Fill extrusion shadows

`org.maplibre.plugins:fill-extrusion-shadows` extends MapLibre's existing `fill-extrusion` layer with one constant paint property:

```json
"fill-extrusion-shadow": true
```

Register the AAR before loading a style that contains the property:

```java
FillExtrusionShadowsPlugin.register();
```

Runtime toggles use the normal MapLibre layer API:

```java
layer.setProperties(FillExtrusionShadows.fillExtrusionShadow(false));
```

The native library supports OpenGL ES and Vulkan in the same renderer-independent AAR. It reuses borrowed fill-extrusion buffers without uploading or retaining MapLibre geometry. Run `generateVulkanShaders` after editing files in `src/main/cpp/shaders`; the task regenerates the checked-in SPIR-V and C include files with the configured Android NDK.

