# Plugin render tests

Each test is an ordinary MapLibre Native render-test directory containing `style.json` and `expected.png`. The custom runner registers the plugin before MapLibre parses any fixture, then delegates to the upstream renderer test harness.

Run the host OpenGL renderer on Linux:

```sh
bazel test //plugins/rectangle-layer:render_tests
```

Build the host Metal runner on macOS:

```sh
bazel build --@maplibre//:renderer=metal //plugins/rectangle-layer:render_tests_metal
```

Rebaseline deliberately by invoking the selected runner with:

```sh
<runner> \
  --manifestPath plugins/rectangle-layer/render-tests/manifest.json --update default
```

The same fixture tree is packaged by the Android and iOS render-test runners. CI should execute it for Android OpenGL, Android Vulkan, and iOS Metal; expected images remain shared unless a backend-specific expectation is necessary.
