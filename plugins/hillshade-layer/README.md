# Hillshade layer

This plugin implements MapLibre Native's built-in hillshade behavior as the
source-bound layer type `org.maplibre.hillshade`. It consumes a `raster-dem`
source and uses the plugin API's host-owned two-pass render graph on OpenGL,
Vulkan, and Metal.

Register the plugin before loading a style, then replace a built-in layer's
`"type": "hillshade"` with `"type": "org.maplibre.hillshade"`. The existing
`hillshade-*` paint properties are unchanged.

## Android

Follow the [shared Android setup](../../README.md#shared-android-setup) to configure
JitPack and a matching plugin-enabled MapLibre SDK. Add this plugin's dependency:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:<matching-maplibre-version>")
    implementation("com.github.louwers.maplibre-native-plugin-template:hillshade-layer:<version-or-commit>")
}
```

For Vulkan, replace `android-sdk-opengl` with `android-sdk-vulkan`; do not add both.
Initialize MapLibre, then register before loading the style:

```kotlin
import org.maplibre.android.MapLibre
import org.maplibre.plugins.hillshade.HillshadeLayerPlugin

MapLibre.getInstance(context)
HillshadeLayerPlugin.register()
```

Build the plugin from the repository root:

```sh
./gradlew :plugins:hillshade-layer:assembleRelease -PmaplibreVersion=<matching-version>
```

There is no dedicated Android gallery activity for this plugin yet. Use its
style layer in your own app or run the plugin render fixtures.

## iOS / Metal

Follow the [shared Apple setup](../../README.md#shared-apple-setup) and select the
`HillshadeLayer` Swift Package Manager product. Link a matching plugin-enabled MapLibre
build. Register before loading a style that uses this plugin:

```swift
import HillshadeLayer

try HillshadeLayerPlugin.registerPlugin()
```

Build the iOS simulator library with Bazel from the repository root:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //plugins/hillshade-layer:HillshadeLayer
```

There is no dedicated iOS gallery scene for this plugin yet. Register it in your
own map app or use its Metal render fixtures.

## Running render tests

Use the [shared runner instructions](../../render-tests/README.md). To run only
this plugin after building the Metal runner:

```sh
./bazel-bin/render_tests_metal \
  --manifestPath plugins/hillshade-layer/render-tests/manifest.json --recycle-map
```
