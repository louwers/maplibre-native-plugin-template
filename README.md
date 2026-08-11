# MapLibre Native plugin template

This is an independent repository for native plugins that either extend existing MapLibre style layers or register new source-less layer types at runtime. It does not include or patch MapLibre Native source code.

The first plugin, `plugins/fill-extrusion-shadows`, registers the constant paint property `fill-extrusion-shadow` for `fill-extrusion` layers and renders projected shadows using geometry borrowed from the host for the duration of each callback. Its implementation is split into `shared`, `android`, and `ios` source trees; platform wrappers do not duplicate descriptor or lifecycle code.

`plugins/gltf-layer` registers the new style layer type `gltf`. It loads GLB data through MapLibre's file loader, parses static meshes with the pinned TinyGLTF submodule, and renders on Android OpenGL/Vulkan and iOS Metal. See its [style example](plugins/gltf-layer/README.md).

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
}
```

Register before constructing or loading a style that contains the property:

```kotlin
FillExtrusionShadowsPlugin.register()
layer.setProperties(FillExtrusionShadows.fillExtrusionShadow(true))

// For a style containing a `gltf` layer:
GltfLayerPlugin.register()
```

The selected MapLibre artifact must contain plugin ABI v1. The plugin POM does not select a renderer transitively.

## Consume on iOS with Swift Package Manager

Add `https://github.com/louwers/maplibre-native-plugin-template` as a package dependency and select the `FillExtrusionShadows` and/or `GltfLayer` product. Link it alongside a MapLibre build that contains plugin ABI v1, then register each plugin before constructing or loading a dependent style:

```swift
import FillExtrusionShadows
import GltfLayer

try FillExtrusionShadowsPlugin.registerPlugin()
try GltfLayerPlugin.registerPlugin()
```

Each GitHub release also includes a prebuilt `FillExtrusionShadows.xcframework.zip` for consumers that do not use Swift Package Manager.

## Build Android locally

The Android plugin is independently buildable. It vendors the versioned pure-C ABI header and compiles its convenience property helper against MapLibre's public Java API without packaging a renderer:

```shell
./gradlew :plugins:fill-extrusion-shadows:assembleRelease \
  -PpluginVersion=0.0.1

./gradlew :plugins:gltf-layer:assembleRelease \
  -PpluginVersion=0.1.0
```

The app under `examples/android-app` has OpenGL and Vulkan product flavors and uses the plugin project directly. Set `maplibreVersion` to a published MapLibre version that contains plugin ABI v1 before running it.

## Build and run iOS with Bazel

The iOS sample consumes the published `0.0.2` plugin XCFramework by checksum and the matching local MapLibre Native checkout through a Bazel `local_path_override`. From this repository:

```shell
bazel build --@maplibre//:renderer=metal \
  --ios_multi_cpus=sim_arm64 \
  //examples/ios-app:FillExtrusionShadowsDemo
```

Install `bazel-bin/examples/ios-app/FillExtrusionShadowsDemo_archive-root/Payload/FillExtrusionShadowsDemo.app` on a booted arm64 Simulator. The sample registers both C plugins before creating `MLNMapView`. Its scene switcher shows either the shadow-enabled Liberty style in Berlin or the remotely loaded Eiffel Tower GLB in Paris.

Validated output is checked in as `screenshots/ios-shadow-enabled.png` and `screenshots/ios-shadow-disabled.png`; the map viewport comparison changes 32,967 pixels along projected building footprints.

The renderer Java API is compile-only and removed from the published POM. MapLibre and plugins may each use a private `c++_static` runtime; only C structs, callbacks, function pointers, and opaque handles cross the boundary. Applications select exactly one renderer artifact themselves.

See [designs/plugin-interface.md](designs/plugin-interface.md) for the API contract and lifecycle. Each plugin has a `release.json`; the `Release plugin` workflow uses that metadata to build the selected Android AAR and iOS XCFramework without plugin-specific workflow branches.
