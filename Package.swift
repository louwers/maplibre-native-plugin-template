// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "MapLibreNativePlugins",
    platforms: [
        .iOS(.v14),
        .macOS(.v10_15),
    ],
    products: [
        .library(
            name: "FillExtrusionShadows",
            targets: ["FillExtrusionShadows"]
        ),
        .library(
            name: "GltfLayer",
            targets: ["GltfLayer"]
        ),
    ],
    targets: [
        .target(
            name: "FillExtrusionShadows",
            path: "plugins/fill-extrusion-shadows",
            exclude: [
                "BUILD.bazel",
                "android",
            ],
            sources: [
                "ios/src/FillExtrusionShadows.mm",
                "ios/src/shadow_metal.mm",
                "shared/cpp/shadow_plugin.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_SHADOW_IOS", to: "1"),
                .define("MLN_SHADOW_PLUGIN_VERSION", to: "\"0.0.2\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
                .linkedFramework("Metal"),
            ]
        ),
        .target(
            name: "TinyGLTF",
            path: "vendor/tinygltf",
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
            dependencies: ["TinyGLTF"],
            path: "plugins/gltf-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "android",
            ],
            sources: [
                "ios/src/GltfLayer.mm",
                "ios/src/gltf_metal.mm",
                "shared/cpp/gltf_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_GLTF_IOS", to: "1"),
                .define("MLN_GLTF_PLUGIN_VERSION", to: "0.1.0"),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
                .linkedFramework("Metal"),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx20
)
