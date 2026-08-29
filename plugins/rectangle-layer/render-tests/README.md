# Rectangle render tests

This directory owns the rectangle plugin's manifest, fixtures, and expected images. It is discovered by the repository-level runner documented in [`render-tests/README.md`](../../../render-tests/README.md).

Run only this plugin's manifest from the repository root with:

```sh
./bazel-bin/render_tests_metal \
  --manifestPath plugins/rectangle-layer/render-tests/manifest.json
```
