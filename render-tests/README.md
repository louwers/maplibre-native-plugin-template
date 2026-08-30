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

The repository-level runner registers every linked plugin, recursively discovers
plugin-owned `render-tests/manifest.json` files, and invokes MapLibre Native's
standard render-test harness once per manifest. Each discovered manifest runs in
an isolated child process so graphics-backend teardown and other process-wide
state cannot leak into the next plugin suite. Adding another fixture to an
existing plugin requires no runner or build change.

Each plugin contributes a tiny self-registering `render-tests/register.cpp` and a
plugin-local CMake definition. CMake discovers those definitions automatically.
For Bazel, expose the standard `render_test_plugin`,
`render_test_plugin_metal`, and `render_test_data` targets and add the package
name once to `plugins/render_tests.bzl`. The shared runner and CI contain no
plugin-specific registration or manifest lists.

On macOS, build and run all discovered Metal suites from the repository root:

```sh
bazel build --@maplibre//:renderer=metal //:render_tests_metal
./bazel-bin/render_tests_metal --plugin-test-root "$PWD" --recycle-map
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

CI exercises every discovered plugin fixture with Linux OpenGL, headless Vulkan,
and Metal. The CMake runner used by the Linux jobs can be reproduced against a
sibling MapLibre Native checkout with:

```sh
cmake -S . -B build-opengl -G Ninja \
  -DMAPLIBRE_NATIVE_SOURCE_DIR="$PWD/../maplibre-native" \
  -DMLN_WITH_OPENGL=ON -DMLN_WITH_X11=ON -DMLN_WITH_WAYLAND=OFF
cmake --build build-opengl --target plugin-render-tests
xvfb-run -a build-opengl/plugin-render-tests \
  --plugin-test-root "$PWD" --recycle-map

cmake -S . -B build-vulkan -G Ninja \
  -DMAPLIBRE_NATIVE_SOURCE_DIR="$PWD/../maplibre-native" \
  -DMLN_WITH_VULKAN=ON -DMLN_WITH_X11=ON -DMLN_WITH_WAYLAND=OFF
cmake --build build-vulkan --target plugin-render-tests
build-vulkan/plugin-render-tests --plugin-test-root "$PWD" --recycle-map
```

Generated `cache.db`, `actual.png`, `diff.png`, and result HTML files are ignored. Generic `expected.png` baselines are committed next to their styles. A plugin whose fixtures need offline resources should commit a deliberately pruned database under a fixture-specific name (for example `fixtures.db`) and select it with `cache_path`.
