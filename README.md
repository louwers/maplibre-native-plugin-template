# MapLibre Native plugin template

Independent, source-bound MapLibre Native layer plugins for Android (OpenGL and
Vulkan) and iOS/macOS (Metal). Plugins register new layer types through a C API;
they do not patch the core or add properties to built-in layers.

- [GLTF](plugins/gltf-layer/README.md): point-anchored GLB meshes loaded through
  MapLibre's file source and parsed with TinyGLTF.
- [N-gon](plugins/ngon-layer/README.md): regular convex markers with thirteen
  fully data-driven paint properties, rotation, and map/viewport alignment.
- [Rectangle](plugins/rectangle-layer/README.md): point markers with dynamic size,
  color, and stroke; the smallest complete drawable-layer example.
- [Hillshade](plugins/hillshade-layer/README.md): RasterDEM-backed
  `org.maplibre.hillshade`, with the built-in hillshade render fixtures.
- [Heatmap](plugins/heatmap-layer/README.md): geometry-backed
  `org.maplibre.heatmap`, with the built-in heatmap render fixtures.

Each plugin separates shared C++ layout, properties, and shader sources from its
Android JNI/Java and iOS Objective-C wrappers. The host owns all GPU resources.
See [the interface design](designs/plugin-interface.md) for ownership and limits.

## Android with JitPack

Use a matching plugin-enabled MapLibre snapshot. Select exactly one renderer;
plugin POMs deliberately do not pull in MapLibre transitively.

```kotlin
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
        maven("https://jitpack.io") {
            content { includeGroup("com.github.louwers.maplibre-native-plugin-template") }
        }
    }
}

dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:<matching-maplibre-version>")
    implementation("com.github.louwers.maplibre-native-plugin-template:rectangle-layer:<version-or-commit>")
}
```

Register before loading a style containing the new type:

```kotlin
RectangleLayerPlugin.register()
// Then load a style with type "rectangle" and a GeoJSON/vector source.
```

Other artifact names are `ngon-layer`, `gltf-layer`, `hillshade-layer`, and `heatmap-layer`.
Their registration wrappers are `NgonLayerPlugin`, `GltfLayerPlugin`, `HillshadeLayerPlugin`, and
`HeatmapLayerPlugin`. This development API is not available in ordinary released
MapLibre SDKs; compile the plugins and host from matching revisions.

## iOS with Swift Package Manager

Add `https://github.com/louwers/maplibre-native-plugin-template` as a package
dependency. Select `NgonLayer`, `GltfLayer`, `RectangleLayer`, `HillshadeLayer`, or `HeatmapLayer`
and link a matching plugin-enabled MapLibre build.

```swift
import RectangleLayer
try RectangleLayerPlugin.registerPlugin()
```

Products share the C-only `MapLibrePluginApi` target. Release CI checks its header
against the selected host revision and publishes an Android AAR and an iOS
XCFramework using each plugin's `release.json` metadata.

`native-revision.txt` pins the matching host for release CI, render CI, and JitPack.
JitPack builds the thin C API locally and publishes every plugin module. It does
not publish or bundle a MapLibre renderer; applications still select one matching
SDK. These changes configure publication, but do not create a new plugin release.

## Local builds and samples

Android plugins compile against MapLibre's renderer-independent
`android-plugin-api` Prefab artifact. Configure the Android SDK in
`local.properties` or `ANDROID_HOME`, then:

```sh
./gradlew :plugins:rectangle-layer:assembleRelease -PmaplibreVersion=<matching-version>
./gradlew :examples:android-app:app:installOpenglDebug -PmaplibreVersion=<matching-version>
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

The gallery has separate activities for the examples. Use `installVulkanDebug`
to select Vulkan instead. Plugin compilation uses the project sources, while the
app's MapLibre dependency must provide the matching API.

The Bazel iOS sample uses the adjacent local MapLibre checkout:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //examples/ios-app:PluginGallery
xcrun simctl install booted \
  bazel-bin/examples/ios-app/PluginGallery_archive-root/Payload/PluginGallery.app
xcrun simctl launch booted org.maplibre.plugins.gallery
```

Use `--override_module=maplibre=/absolute/path/to/maplibre-native` when the host
is elsewhere. The iOS scene selector retains the Eiffel Tower and rectangle
examples and adds n-gons; `SIMCTL_CHILD_PLUGIN_SCENE=model`, `rectangle`, or `ngon`
selects the initial scene.

## Render tests

Fixtures and reviewed `expected.png` images belong to their plugin under
`plugins/<plugin>/render-tests`. A shared executable discovers all manifests and
delegates to MapLibre's standard render-test harness; there are no per-plugin
branches or exclusions in the runner.

```sh
bazel build --@maplibre//:renderer=metal //:render_tests_metal
./bazel-bin/render_tests_metal --plugin-test-root "$PWD"
```

See [render-tests/README.md](render-tests/README.md) for the OpenGL/Vulkan CMake
targets, filtering, and baseline review. Draft PRs skip expensive render jobs;
marking them ready starts normal CI.
