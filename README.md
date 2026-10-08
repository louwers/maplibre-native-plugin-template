# MapLibre Native plugin template

A starting point for a [MapLibre Native](https://github.com/maplibre/maplibre-native) layer plugin
for Android and iOS. It contains one small, working plugin: the `square` layer type, which draws a
screen-aligned square at every point of a source.

```json
{"id": "dots", "type": "square", "source": "points",
 "paint": {"square-color": ["get", "color"], "square-size": 24}}
```

Builds, demo apps, render tests and releases come from
[maplibre-native-plugins](https://github.com/louwers/maplibre-native-plugins), included as the `tools`
submodule. For a complete plugin built from this template, see
[maplibre-native-ngon-layer-plugin](https://github.com/louwers/maplibre-native-ngon-layer-plugin).

## Start a plugin

1. Select **Use this template > Create a new repository** on GitHub (or fork this repository), then clone
   it with its submodule:

   ```sh
   gh repo create my-layer-plugin --public --clone --template louwers/maplibre-native-plugin-template
   cd my-layer-plugin && git submodule update --init
   ```

2. Rename the example. Everything plugin-specific is in these files:
   - `plugin.json`: `id`, `displayName`, `description`, `registerFunction`, `header`, `sources`.
     Platform names (Android class and package, Swift product, Objective-C class) are derived
     from `id`; override them under `android` and `apple` if needed.
   - `include/square_layer.h` and `src/square_layer.cpp`: the plugin. Rename the files and the
     `mln_square_layer_register` function, then change the plugin ID (`org.maplibre.square-layer`),
     the layer type (`square`), its paint properties and its shaders.
   - `examples/square.json`: the style the demo apps load (`demo.style` in `plugin.json`).
   - `render-tests/`: one directory per test with a `style.json` and the reviewed `expected.png`.
3. Regenerate the Swift package and the iOS wrapper with `tools/bin/plugin sync`, and check the derived
   names with `tools/bin/plugin config`.

## How the example works

`src/square_layer.cpp` is commented from top to bottom. In short:

- **Registration.** `mln_square_layer_register` passes a `mln_plugin_descriptor_v1` to MapLibre. It
  declares the layer type, its paint properties with defaults, and one shader with sources for
  OpenGL, Vulkan and Metal.
- **Layout.** On worker threads, MapLibre calls `layoutFeature` for every point feature in a tile.
  The plugin emits four vertices and two triangles per point.
- **Paint properties.** Property bindings tell MapLibre where to put evaluated values: in the
  uniform block for constants and zoom expressions, or in vertex attributes for data-driven
  expressions. The shaders select the right source with the
  `MLN_PLUGIN_PROPERTY_<NAME>_IS_UNIFORM` macros MapLibre defines.
- **Rendering.** Every frame, `updateUniform` writes the tile matrix and pixel scale. MapLibre owns
  all GPU resources and draws the triangles.

## Develop

```sh
tools/bin/plugin run-android --renderer vulkan   # demo app on a connected device or emulator
tools/bin/plugin run-ios                         # demo app on an iOS simulator
tools/bin/plugin render-tests                    # Metal on macOS; --backend opengl|vulkan on Linux
tools/bin/plugin render-tests -- --update default   # record expected.png files, then review them
```

CI checks that generated files are current, builds both platforms, starts the iOS demo app and runs the
render tests on Metal, OpenGL and Vulkan. See the
[tools documentation](https://github.com/louwers/maplibre-native-plugins#readme) for all commands.

## Release

Set `version` in `plugin.json`, run `tools/bin/plugin generate`, commit, and run the **Release**
workflow. It publishes an Android AAR and an iOS XCFramework to a GitHub release tagged with the
version. Swift Package Manager users can also depend on the repository directly at that tag.

## Update the tools

```sh
git -C tools fetch --tags && git -C tools checkout <tag>
tools/bin/plugin sync     # refreshes workflows, Gradle files and the Swift package
git add -A && git commit -m "Update plugin tools to <tag>"
```
