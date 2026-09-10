# Rectangle layer

The rectangle plugin registers a source-bound `rectangle` style layer. It consumes Point and MultiPoint features from GeoJSON or vector-tile geometry sources. Every point becomes a centered, viewport-aligned rectangle measured in logical screen pixels.

```json
{
  "id": "points",
  "type": "rectangle",
  "source": "points",
  "paint": {
    "rectangle-color": "#e00",
    "rectangle-width": 12,
    "rectangle-height": 12,
    "rectangle-stroke-width": 1,
    "rectangle-stroke-color": "#000"
  }
}
```

All five paint properties accept MapLibre expressions. The host evaluates them per feature during tile layout, copies the returned CPU bucket, compiles the plugin-provided OpenGL/Vulkan/Metal shader, and owns every resulting GPU buffer and drawable. Register the plugin before loading a style containing the layer.

## Android

Follow the [shared Android setup](../../README.md#shared-android-setup) to configure
JitPack and a matching plugin-enabled MapLibre SDK. Add this plugin's dependency:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:<matching-maplibre-version>")
    implementation("com.github.louwers.maplibre-native-plugin-template:rectangle-layer:<version-or-commit>")
}
```

For Vulkan, replace `android-sdk-opengl` with `android-sdk-vulkan`; do not add both.
Initialize MapLibre, then register before loading the style:

```kotlin
import org.maplibre.android.MapLibre
import org.maplibre.plugins.rectangle.RectangleLayerPlugin

MapLibre.getInstance(context)
RectangleLayerPlugin.register()
```

Build the plugin from the repository root:

```sh
./gradlew :plugins:rectangle-layer:assembleRelease -PmaplibreVersion=<matching-version>
```

### Android example

Install the gallery and choose **Rectangle layer**:

```sh
./gradlew :examples:android-app:app:installOpenglDebug -PmaplibreVersion=<matching-version>
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installVulkanDebug` to run the Vulkan variant.

The Android sample demonstrates feature-driven paint and smoothly interpolated
zoom styling, with a button to animate between zoom levels 10 and 16.

## iOS / Metal

Follow the [shared Apple setup](../../README.md#shared-apple-setup) and select the
`RectangleLayer` Swift Package Manager product. Link a matching plugin-enabled MapLibre
build. Register before loading a style that uses this plugin:

```swift
import RectangleLayer

try RectangleLayerPlugin.registerPlugin()
```

Build the iOS simulator library with Bazel from the repository root:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //plugins/rectangle-layer:RectangleLayer
```

### iOS example

With an iOS simulator booted, build and run this plugin's gallery scene:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //examples/ios-app:PluginGallery
xcrun simctl install booted \
  bazel-bin/examples/ios-app/PluginGallery_archive-root/Payload/PluginGallery.app
SIMCTL_CHILD_PLUGIN_SCENE=rectangle xcrun simctl launch booted org.maplibre.plugins.gallery
```

## Running render tests

Use the [shared runner instructions](../../render-tests/README.md). To run only
this plugin after building the Metal runner:

```sh
./bazel-bin/render_tests_metal \
  --manifestPath plugins/rectangle-layer/render-tests/manifest.json --recycle-map
```
