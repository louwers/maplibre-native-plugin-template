# GLTF layer

Registers a source-bound `gltf` style layer backed by TinyGLTF. Point features provide the geographic anchors. Model
downloads run during CPU layout through MapLibre Native's file loader, including its network policy, resource
transformation, and cache. The plugin emits backend-neutral buckets and MapLibre owns every GPU drawable.

```json
{
  "id": "eiffel-tower",
  "type": "gltf",
  "source": "eiffel-tower-anchor",
  "minzoom": 14,
  "layout": {
    "model-uri": "https://3dmr.eu/api/model/4/3",
    "model-altitude": 0,
    "model-heading": 0,
    "model-scale": 3.2
  },
  "paint": { "model-opacity": 1 }
}
```

Here `eiffel-tower-anchor` is a GeoJSON source containing a point at `[2.2944962, 48.8582621]`. One model instance is
laid out for every point feature, so normal source filtering, tiling, visibility, zoom ranges, and layer ordering apply.

The initial implementation supports static indexed triangle meshes, node transforms, material base colors, alpha
blending, and GLB files. Animation, skinning, morph targets, textures, mesh compression, and terrain anchoring are
deferred.

## Android

Follow the [shared Android setup](../../README.md#shared-android-setup) to configure
JitPack and a matching plugin-enabled MapLibre SDK. Add this plugin's dependency:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:<matching-maplibre-version>")
    implementation("com.github.louwers.maplibre-native-plugin-template:gltf-layer:<version-or-commit>")
}
```

For Vulkan, replace `android-sdk-opengl` with `android-sdk-vulkan`; do not add both.
Initialize MapLibre, then register before loading the style:

```kotlin
import org.maplibre.android.MapLibre
import org.maplibre.plugins.gltf.GltfLayerPlugin

MapLibre.getInstance(context)
GltfLayerPlugin.register()
```

Build the plugin from the repository root:

```sh
git submodule update --init --recursive
./gradlew :plugins:gltf-layer:assembleRelease -PmaplibreVersion=<matching-version>
```

### Android example

Install the gallery and choose **Eiffel Tower GLTF**:

```sh
./gradlew :examples:android-app:app:installOpenglDebug -PmaplibreVersion=<matching-version>
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installVulkanDebug` to run the Vulkan variant.

The Android sample uses OpenFreeMap Positron and the remote Eiffel Tower GLB
shown above. Model and map downloads require an Internet connection.

## iOS / Metal

Follow the [shared Apple setup](../../README.md#shared-apple-setup) and select the
`GltfLayer` Swift Package Manager product. Link a matching plugin-enabled MapLibre
build. Register before loading a style that uses this plugin:

```objc
#import <GltfLayer/GltfLayer.h>

NSError *error = nil;
if (![MLNGltfLayerPlugin registerPluginWithError:&error]) {
    NSLog(@"GLTF registration failed: %@", error);
    return;
}
```

Source builds require the TinyGLTF submodule; initialize submodules before building.

Build the iOS simulator library with Bazel from the repository root:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //plugins/gltf-layer:GltfLayer
```

### iOS example

With an iOS simulator booted, build and run this plugin's gallery scene:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  //examples/ios-app:PluginGallery
xcrun simctl install booted \
  bazel-bin/examples/ios-app/PluginGallery_archive-root/Payload/PluginGallery.app
SIMCTL_CHILD_PLUGIN_SCENE=model xcrun simctl launch booted org.maplibre.plugins.gallery
```

## Running render tests

Use the [shared runner instructions](../../render-tests/README.md). To run only
this plugin after building the Metal runner:

```sh
./bazel-bin/render_tests_metal \
  --manifestPath plugins/gltf-layer/render-tests/manifest.json --recycle-map
```
