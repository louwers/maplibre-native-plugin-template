.target(
            name: "TinyGLTF",
            path: "plugins/gltf-layer/vendor/tinygltf",
            exclude: [
                "CMakeLists.txt", "LICENSE", "Makefile", "README.md", "appveyor.yml", "cmake", "examples",
                "examples.bat", "experimental", "loader_example.cc", "models", "premake5.lua", "test_runner.py",
                "tests", "vcsetup.bat", "wasm",
            ],
            sources: ["tiny_gltf.cc"],
            publicHeadersPath: "."
        ),
.target(
            name: "GltfLayer",
            dependencies: [.product(name: "MapLibrePluginApi", package: "maplibre-native"), "TinyGLTF"],
            path: "plugins/gltf-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "plugin.json", "swift-targets.swift", "examples",
                "android",
            ],
            sources: [
                "ios/src/GltfLayer.mm",
                "shared/cpp/gltf_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_GLTF_PLUGIN_VERSION", to: "\"0.1.0\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
