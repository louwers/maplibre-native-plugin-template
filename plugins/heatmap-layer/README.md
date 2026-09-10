# Heatmap layer plugin

This plugin registers the source-bound style layer type `org.maplibre.heatmap`.
It implements the built-in MapLibre heatmap behavior through the C plugin API:
point layout, data-driven radius and weight, a half-resolution floating-point
density target, additive Gaussian kernels, and color-ramp compositing.

The built-in `heatmap` layer remains available. Register this plugin before
loading a style that contains `"type": "org.maplibre.heatmap"`.

## Android

Follow the [shared Android setup](../../README.md#shared-android-setup) to configure
JitPack and a matching plugin-enabled MapLibre SDK. Add this plugin's dependency:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:<matching-maplibre-version>")
    implementation("com.github.louwers.maplibre-native-plugin-template:heatmap-layer:<version-or-commit>")
}
```

For Vulkan, replace `android-sdk-opengl` with `android-sdk-vulkan`; do not add both.
Initialize MapLibre, then register before loading the style:

```kotlin
import org.maplibre.android.MapLibre
import org.maplibre.plugins.heatmap.HeatmapLayerPlugin

MapLibre.getInstance(context)
HeatmapLayerPlugin.register()
```

Build the plugin from the repository root:

```sh
./gradlew :plugins:heatmap-layer:assembleRelease -PmaplibreVersion=<matching-version>
```

### Android example

Install the gallery and choose **Heatmap layer**:

```sh
./gradlew :examples:android-app:app:installOpenglDebug -PmaplibreVersion=<matching-version>
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installVulkanDebug` to run the Vulkan variant.

The Android sample renders density from GeoJSON points using the plugin layer
type, rather than the built-in heatmap layer.

## iOS / Metal

Follow the [shared Apple setup](../../README.md#shared-apple-setup) and select the
`HeatmapLayer` Swift Package Manager product. Link a matching plugin-enabled MapLibre
build. Register before loading a style that uses this plugin:

```swift
import HeatmapLayer

try HeatmapLayerPlugin.registerPlugin()
```

Build the iOS simulator library with Bazel from the repository root:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //plugins/heatmap-layer:HeatmapLayer
```

There is no dedicated iOS gallery scene for this plugin yet. Register it in your
own map app or use its Metal render fixtures.

## Running render tests

Use the [shared runner instructions](../../render-tests/README.md). To run only
this plugin after building the Metal runner:

```sh
./bazel-bin/render_tests_metal \
  --manifestPath plugins/heatmap-layer/render-tests/manifest.json --recycle-map
```
