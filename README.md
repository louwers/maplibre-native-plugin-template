# MapLibre Native plugin template

This is an independent repository for native plugins that either extend existing MapLibre style layers or register new source-bound layer types at runtime. It does not include or patch MapLibre Native source code.

The first plugin, `plugins/fill-extrusion-shadows`, registers the constant paint property `fill-extrusion-shadow` for `fill-extrusion` layers and renders projected shadows using geometry borrowed from the host for the duration of each callback. Its implementation is split into `shared`, `android`, and `ios` source trees; platform wrappers do not duplicate descriptor or lifecycle code.

`plugins/gltf-layer` registers the source-bound style layer type `gltf`. It loads GLB data through MapLibre's file loader during layout, parses static meshes with the pinned TinyGLTF submodule, and creates host-owned drawables using plugin-registered OpenGL, Vulkan, and Metal shaders. See its [style example](plugins/gltf-layer/README.md).

`plugins/rectangle-layer` is the minimal source-bound example. Point features become screen-space rectangles with expression-capable fill, size, and stroke properties. Its render-test directory demonstrates how an independent plugin registers itself before delegating fixtures to MapLibre's standard render-test harness.

`plugins/hillshade-layer` registers the RasterDEM-backed type `org.maplibre.hillshade`. It reproduces the built-in hillshade layer through a host-owned two-pass render graph and explicit OpenGL, Vulkan, and Metal shader resources; the built-in `hillshade` type remains unchanged.

## Consume on Android with JitPack

Choose exactly one plugin-enabled MapLibre renderer and add JitPack after your normal repositories:

```kotlin
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
        maven("https://jitpack.io") {
            content {
                includeGroup("com.github.louwers")
            }
        }
    }
}
```

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:<plugin-enabled-maplibre-version>")
    implementation("com.github.louwers.maplibre-native-plugin-template:fill-extrusion-shadows:<version>")
    // or: implementation("com.github.louwers.maplibre-native-plugin-template:gltf-layer:<version>")
    // or: implementation("com.github.louwers.maplibre-native-plugin-template:rectangle-layer:<version>")
    // or: implementation("com.github.louwers.maplibre-native-plugin-template:hillshade-layer:<version>")
}
```

Register before constructing or loading a style that contains the property:

```kotlin
FillExtrusionShadowsPlugin.register()
layer.setProperties(FillExtrusionShadows.fillExtrusionShadow(true))

// For a style containing a `gltf` layer:
GltfLayerPlugin.register()

// For a raster-dem style layer whose type is `org.maplibre.hillshade`:
HillshadeLayerPlugin.register()
```

The selected MapLibre artifact must contain plugin ABI v1. The plugin POM does not select a renderer transitively.

## Consume on iOS with Swift Package Manager

Add `https://github.com/louwers/maplibre-native-plugin-template` as a package dependency and select the `FillExtrusionShadows`, `GltfLayer`, `HillshadeLayer`, and/or `RectangleLayer` product. Link it alongside a MapLibre build that contains plugin ABI v1, then register each plugin before constructing or loading a dependent style:

```swift
import FillExtrusionShadows
import GltfLayer
import RectangleLayer
import HillshadeLayer

try FillExtrusionShadowsPlugin.registerPlugin()
try GltfLayerPlugin.registerPlugin()
try RectangleLayerPlugin.registerPlugin()
try HillshadeLayerPlugin.registerPlugin()
```

Each GitHub release also includes a prebuilt XCFramework for its selected plugin for consumers that do not use Swift Package Manager.

SwiftPM products share one header-only `MapLibrePluginApi` target. Release CI compares that packaged C header byte-for-byte with the selected MapLibre Native checkout, preventing a plugin release from compiling against a stale callback or struct layout.

## Build Android locally

The Android plugins are independently buildable. Native compilation consumes the canonical pure-C header from MapLibre's `android-plugin-api` Prefab artifact without packaging a renderer:

```shell
./gradlew :plugins:fill-extrusion-shadows:assembleRelease \
  -PpluginVersion=0.0.1

./gradlew :plugins:gltf-layer:assembleRelease \
  -PpluginVersion=0.1.0

./gradlew :plugins:hillshade-layer:assembleRelease \
  -PpluginVersion=0.1.0
```

The app under `examples/android-app` has OpenGL and Vulkan product flavors and uses the plugin project directly. Set `maplibreVersion` to a published MapLibre version that contains plugin ABI v1 before running it.

## Build and run iOS with Bazel

The iOS sample builds the shadow, GLTF, and rectangle plugins from their shared sources and uses the matching local MapLibre Native checkout through a Bazel `local_path_override`. From this repository:

```shell
bazel build --@maplibre//:renderer=metal \
  --ios_multi_cpus=sim_arm64 \
  //examples/ios-app:FillExtrusionShadowsDemo
```

Install `bazel-bin/examples/ios-app/FillExtrusionShadowsDemo_archive-root/Payload/FillExtrusionShadowsDemo.app` on a booted arm64 Simulator. The sample registers all plugins before creating `MLNMapView`. Its scene switcher shows the shadow-enabled Liberty style in Berlin, the remotely loaded Eiffel Tower GLB in Paris, and the source-bound rectangle layer. For deterministic launches, set `SIMCTL_CHILD_PLUGIN_SCENE` to `shadows`, `model`, or `rectangle` when invoking `xcrun simctl launch`.

Validated output is checked in as `screenshots/ios-shadow-enabled.png` and `screenshots/ios-shadow-disabled.png`; the map viewport comparison changes 32,967 pixels along projected building footprints.

The renderer Java API is compile-only and removed from the published POM. MapLibre and plugins may each use a private `c++_static` runtime; only C structs, callbacks, function pointers, and opaque handles cross the boundary. Applications select exactly one renderer artifact themselves.

## Render tests

Render fixtures and committed `expected.png` images live with the plugin that owns them under `plugins/<plugin>/render-tests`. One repository-level executable registers the linked plugins, discovers every available manifest, and runs MapLibre Native's standard render-test harness for each suite. Build and run the Metal configuration with:

```shell
bazel build --@maplibre//:renderer=metal //:render_tests_metal
./bazel-bin/render_tests_metal --plugin-test-root "$PWD"
```

See [render-tests/README.md](render-tests/README.md) for filtering, rebaselining, and the portable Linux target. The same commands run in the `Plugin render tests` workflow.

See [designs/plugin-interface.md](designs/plugin-interface.md) for the API contract and lifecycle. Each plugin has a `release.json`; the `Release plugin` workflow uses that metadata to build the selected Android AAR and iOS XCFramework without plugin-specific workflow branches.
