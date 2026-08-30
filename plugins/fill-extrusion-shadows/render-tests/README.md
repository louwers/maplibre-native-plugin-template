# Fill-extrusion shadow render tests

These fixtures use asymmetric, locally defined GeoJSON buildings so shadow
direction, bearing/pitch camera transforms, and data-driven base/height
projection are deterministic and do not depend on network tiles. Run them
through the repository-level runner:

```sh
./bazel-bin/render_tests_metal \
  --manifestPath plugins/fill-extrusion-shadows/render-tests/manifest.json
```

The repository workflow also runs this manifest through the shared Linux
OpenGL and headless Vulkan CMake runner. See
[`render-tests/README.md`](../../../render-tests/README.md) for the equivalent
local commands.
