# Shared plugin render-test runner

Every plugin owns its fixtures and expectations under:

```text
plugins/<plugin>/render-tests/
├── manifest.json
├── results/.gitkeep
└── <suite>/<case>/
    ├── style.json
    └── expected.png
```

The repository-level runner registers the linked plugins, discovers every `plugins/*/render-tests/manifest.json`, and invokes MapLibre Native's standard render-test harness once per manifest. Adding another fixture to an existing plugin requires no runner or BUILD change.

To make a new plugin available to the runner, expose its C registration function from a Bazel library, add that library to the two runner dependency lists in the root `BUILD.bazel`, and add its directory and registration function to `render-tests/main.cpp`.

On macOS, build and run all discovered Metal suites from the repository root:

```sh
bazel build --@maplibre//:renderer=metal //:render_tests_metal
./bazel-bin/render_tests_metal --plugin-test-root "$PWD"
```

List the discovered manifests without rendering:

```sh
./bazel-bin/render_tests_metal --plugin-test-root "$PWD" --list-plugin-tests
```

Arguments understood by MapLibre's runner, including `--filter`, `--online`, and `--update default`, are forwarded to every discovered manifest. Pass `--manifestPath <path>` to run only one plugin manifest.

The portable Linux OpenGL Bazel target is:

```sh
bazel test //:render_tests
```

CI exercises the fill-extrusion shadow fixtures with both Linux OpenGL and
headless Vulkan. The CMake runner used by those jobs can be reproduced against
a sibling MapLibre Native checkout with:

```sh
cmake -S . -B build-opengl -G Ninja \
  -DMAPLIBRE_NATIVE_SOURCE_DIR="$PWD/../maplibre-native" \
  -DMLN_WITH_OPENGL=ON -DMLN_WITH_X11=ON -DMLN_WITH_WAYLAND=OFF
cmake --build build-opengl --target plugin-render-tests
xvfb-run -a build-opengl/plugin-render-tests \
  --manifestPath "$PWD/plugins/fill-extrusion-shadows/render-tests/manifest.json"

cmake -S . -B build-vulkan -G Ninja \
  -DMAPLIBRE_NATIVE_SOURCE_DIR="$PWD/../maplibre-native" \
  -DMLN_WITH_VULKAN=ON -DMLN_WITH_X11=ON -DMLN_WITH_WAYLAND=OFF
cmake --build build-vulkan --target plugin-render-tests
build-vulkan/plugin-render-tests \
  --manifestPath "$PWD/plugins/fill-extrusion-shadows/render-tests/manifest.json"
```

Generated `cache.db`, `actual.png`, `diff.png`, and result HTML files are ignored. Generic `expected.png` baselines are committed next to their styles. A plugin whose fixtures need offline resources should commit a deliberately pruned database under a fixture-specific name (for example `fixtures.db`) and select it with `cache_path`.
