# MapLibre Native plugin template

Infrastructure for independent, source-bound MapLibre Native layer plugins on
Android (OpenGL and Vulkan) and iOS/macOS (Metal). Plugins register new layer types
through a C API; they do not patch the core or add properties to built-in layers.

Each directory under [plugins/](plugins/) owns its implementation and README.
Start with that plugin's README for its style schema, platform-specific dependency
coordinates, registration API, build targets, and available examples.

## Repository structure

Plugins separate shared C++ layout, properties, and shader sources from Android
JNI/Java and iOS Objective-C wrappers. The host owns all GPU resources.

- `plugins/<plugin>/shared`: cross-platform implementation.
- `plugins/<plugin>/android` and `ios`: platform wrappers.
- `plugins/<plugin>/render-tests`: fixtures and reviewed expected images.
- `examples/`: platform sample applications.

Architecture designs live in MapLibre Native under `design-proposals/plugin-api/`.

## Host compatibility

This development API is not available in ordinary released MapLibre SDKs. Build
plugins and the host from matching revisions. `native-revision.txt` pins the host
used by release CI, render CI, and JitPack.

On Android, select exactly one matching MapLibre renderer artifact. Plugins use
the renderer-independent `android-plugin-api` Prefab dependency; they do not
bundle or select a renderer. On Apple platforms, link a matching plugin-enabled
MapLibre build alongside the selected plugin product.

Always register plugins before loading a style that uses their layer types.
Registration entry points are documented in each plugin's README.

## Shared Android setup

Add JitPack to the application's dependency repositories:

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
```

Use the artifact name and registration example from the selected plugin's README.
Choose either `org.maplibre.gl:android-sdk-opengl:<matching-maplibre-version>` or
`org.maplibre.gl:android-sdk-vulkan:<matching-maplibre-version>` for the host.
Configure the repository hosting that matching SDK if it is not in Maven Central.

For local builds, configure the Android SDK in `local.properties` or
`ANDROID_HOME`. Commands in plugin READMEs run from this repository's root.
Set `-PmaplibreVersion=<matching-version>` and, when needed,
`-PmaplibreRepositoryUrl=<repository-url>` to resolve the matching host artifacts.

## Shared Apple setup

Add `https://github.com/louwers/maplibre-native-plugin-template` as a Swift Package
Manager dependency. Each plugin README identifies its product, import, registration
API, and Bazel build target. Products depend on the C-only `MapLibrePluginApi` product from MapLibre Native.

For Bazel builds, the sample uses the adjacent local MapLibre checkout. See
[Developing the native plugin API locally](#developing-the-native-plugin-api-locally)
for checkout overrides. Plugin READMEs document available sample scenes; not
every plugin has a gallery scene on every platform.

## Developing the native plugin API locally

Use a plugin-enabled MapLibre Native checkout alongside this repository. Start
from a compatible branch or the revision in `native-revision.txt`; an ordinary
released SDK does not contain this API. Local builds can use uncommitted changes,
so there is no need to publish to a remote Maven repository or make a release
while iterating.

Run the following from this repository's root, adjusting the native path if needed:

```sh
plugin_root="$PWD"
native_root="$(cd ../maplibre-native && pwd)"
git -C "$native_root" submodule update --init --recursive
git submodule update --init --recursive
```

### Edit the host API

The public contract lives in `include/mln/plugin/plugin_api.h` in MapLibre Native.
Host registration, factories, and shader adapters live under `src/mln/plugin/`;
render integration lives in `src/mln/renderer/layers/render_plugin_style_layer.*`
and `plugin_layer_tweaker.*`. Change the host implementation and plugin callers
together, keeping the boundary pure C.

There is no copied API header in this repository. Android consumes it through
the MapLibre Prefab API artifact; Bazel consumes the host's plugin API target.
SwiftPM consumes MapLibre Native's `MapLibrePluginApi` product at the revision in
`native-revision.txt`. That revision must include the host's Swift package manifest.
For local SwiftPM development, select your checkout explicitly:

```sh
MAPLIBRE_NATIVE_PATH="$native_root" swift build --target MapLibrePluginApi
```

Use the same environment variable when building any plugin target. In Xcode,
add the local MapLibre Native package as an override for the remote dependency.
The API product supplies only the C contract, not a renderer: applications must
still link a matching MapLibre SDK. Rebuild both SDK and plugins after changing
the contract; replacing only one native library can leave incompatible binaries.

### Android: publish matching artifacts locally

Configure `ANDROID_HOME` (or `local.properties`) and the required JDK/NDK for the
Android builds. Use a development version shared by the host SDK and API artifact:

```sh
api_version="0.0.0-plugin-local-SNAPSHOT"
"$native_root/platform/android/gradlew" -p "$native_root/platform/android" \
  :android-plugin-api:publishReleasePublicationToMavenLocal \
  :MapLibreAndroid:publishVulkanreleasePublicationToMavenLocal \
  -PmaplibreVersion="$api_version" \
  -Pmaplibre.abis=arm64-v8a \
  -PpublicationRepositoryUrl="file://$plugin_root/build/local-maven"
```

These tasks publish only to the local Maven repository, normally
`$HOME/.m2/repository`. The file-valued publication setting avoids enabling the
Maven Central publishing/signing configuration for this development build; it
does not redirect `publishToMavenLocal`. No remote credentials are needed.
For OpenGL, use `publishOpenglreleasePublicationToMavenLocal` instead. Replace
`arm64-v8a` with your device/emulator ABI as appropriate, or `all` for the SDK's
full ABI set.

Then build and install the example against those exact local artifacts:

```sh
cd "$plugin_root"
MAPLIBRE_REPOSITORY_URL="file://$HOME/.m2/repository" ./gradlew \
  :examples:android-app:app:installVulkanDebug \
  -PmaplibreVersion="$api_version" \
  -PmaplibrePluginAbis=arm64-v8a \
  --refresh-dependencies
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installOpenglDebug` with the OpenGL artifact. The example builds plugins from
this checkout while resolving the host SDK and C API from local Maven. Republishing
the same snapshot requires refreshing dependencies; using a fresh development
version for each API change also avoids stale artifacts. A local Swift package
dependency similarly uses this repository's current sources, but you must still
build and link the matching MapLibre host separately.

### Bazel / iOS: build directly against the checkout

Bazel can rebuild host and plugin source changes together without Maven artifacts.
Override both the native module and its tile-spec submodule when using a different
checkout location; dependency-module overrides are not inherited by the root module:

```sh
bazel build --@maplibre//:renderer=metal --ios_multi_cpus=sim_arm64 \
  --override_module="maplibre=$native_root" \
  --override_module="maplibre-tile-spec=$native_root/vendor/maplibre-tile-spec" \
  //examples/ios-app:PluginGallery
```

Use the plugin-specific library target and simulator launch instructions in its
README when you do not need the whole gallery.

### Test and share API changes

Run the host's focused plugin tests as well as the plugin-owned render fixtures.
For example, on macOS the CMake runner builds directly against your working tree:

```sh
cmake -S . -B build-api-metal -G Ninja \
  -DMAPLIBRE_NATIVE_SOURCE_DIR="$native_root" -DMLN_WITH_METAL=ON
cmake --build build-api-metal --target plugin-render-tests
./build-api-metal/plugin-render-tests --plugin-test-root "$plugin_root" --recycle-map
```

See the [shared render-test guide](render-tests/README.md) for OpenGL and Vulkan.
Repeat the local build after host changes; release CI and JitPack cannot see
uncommitted files or local Maven artifacts. Once the host changes are committed
and pushed, update `native-revision.txt` to that accessible commit and include
the synchronized C header and plugin-side changes in review. Do not replace the
release pin with a local path or an unpublished commit.

## Publishing

Release CI checks the C header against the selected host revision and publishes
an Android AAR and an iOS XCFramework using each plugin's `release.json` metadata.
The release workflow selects the plugin and version to publish.

JitPack builds the thin C API locally and publishes the plugin modules. It does
not publish or bundle a MapLibre renderer. Publication configuration alone does
not mean a particular plugin version has been released.

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
