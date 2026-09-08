"""The single Bazel catalog of plugin packages that provide render tests.

Bazel does not support discovering targets across package boundaries. CMake and
the runner discover plugins and manifests automatically; adding a Bazel package
requires adding its package name here once.
"""

RENDER_TEST_PLUGIN_PACKAGES = [
    "gltf-layer",
    "heatmap-layer",
    "hillshade-layer",
    "rectangle-layer",
]

def render_test_plugin_targets(target):
    return ["//plugins/{}:{}".format(package, target) for package in RENDER_TEST_PLUGIN_PACKAGES]
